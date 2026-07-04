#include "apic.h"

#include <uacpi/acpi.h>
#include <uacpi/tables.h>
#include <uacpi/event.h>
#include <uacpi/utilities.h>
#include <uacpi/resources.h>
#include "../utilities/util.h"
#include "../memory/memory.h"
#include "../memory/kmalloc.h"
#include "../tables/interrupts.h"
#include "../io/log.h"
#include "../drivers/PCI/pci.h"


vector_t ioapics;
vector_t int_src_overrides;
apic_data_t lapic_data;
bool found_lapic = false;

void write_ioapic_register(uintptr_t ioapic_base,uint8_t offset, uint32_t val){
    // select register via IOREGSEL
    *(volatile uint32_t*)(ioapic_base) = offset;
    // write to IOWIN
    *(volatile uint32_t*)(ioapic_base + 0x10) = val;
}

uint32_t read_ioapic_register(uintptr_t ioapic_base,uint8_t offset){
    *(volatile uint32_t*)(ioapic_base) = offset;

    return *(volatile uint32_t*)(ioapic_base + 0x10);
}

void write_apic_register(uint32_t offset, uint32_t value){
    *(volatile uint32_t*)(lapic_data.apic_virt_base + offset) = value;
}

uint32_t read_apic_register(uint32_t offset){
    return *(volatile uint32_t*)(lapic_data.apic_virt_base + offset);
}
uint8_t ioapic_get_redir_entry_cnt(uintptr_t apic_base){
    return (read_ioapic_register(apic_base,IOAPICVER) >> 16) + 1;
}

void dump_ioapic_info(uintptr_t apic_base){
    uint8_t apicid = (read_ioapic_register(apic_base,IOAPICID) >> 24) & 0xf;
    logf("IO APIC Id: %d",apicid);
    uint8_t redir_entry_cnt = ioapic_get_redir_entry_cnt(apic_base);
    logf("This IO APIC supports up to %d redirections",redir_entry_cnt);
}

uacpi_iteration_decision madt_callback(void* user, struct acpi_entry_hdr* hdr){

    switch (hdr->type)
    {
    case ACPI_MADT_ENTRY_TYPE_LAPIC: ;
        if (found_lapic) {warn("Found more than one local APIC on one CPU??");break;}
        struct acpi_madt_lapic* lapic = (struct acpi_madt_lapic*)hdr;
        logf("Found Local APIC with id: %d",lapic->id);
        logf("ACPI Processor ID: %d",lapic->uid);
        lapic_data.apic_id = lapic->id;
        lapic_data.acpi_processor_id = lapic->uid;
        found_lapic = true;
        break;
    case ACPI_MADT_ENTRY_TYPE_INTERRUPT_SOURCE_OVERRIDE: ;
        struct acpi_madt_interrupt_source_override* iso = (struct acpi_madt_interrupt_source_override*)hdr;
        logf("Interrupt source override: %d -> %d",iso->source,iso->gsi);
        struct acpi_madt_interrupt_source_override* iso_save = (struct acpi_madt_interrupt_source_override*)kmalloc(sizeof(struct acpi_madt_interrupt_source_override));
        
        memcpy(iso_save,iso,sizeof(struct acpi_madt_interrupt_source_override));
        vector_append(&int_src_overrides,(vector_data_t)iso_save);
        break;
    case ACPI_MADT_ENTRY_TYPE_IOAPIC: ;
        // found an IO APIC
        struct acpi_madt_ioapic* ioapic = (struct acpi_madt_ioapic*)hdr;
        logf("Found IO APIC at address %x",ioapic->address); // base for MMIO
        mem_map_page(ioapic->address + HHDM,ioapic->address,PAGE_FLAG_WRITE);

        struct acpi_madt_ioapic* ioapic_save = (struct acpi_madt_ioapic*)kmalloc(sizeof(struct acpi_madt_ioapic));
        memcpy(ioapic_save,ioapic,sizeof(struct acpi_madt_ioapic));

        vector_append(&ioapics,(vector_data_t)ioapic_save);
        dump_ioapic_info(ioapic->address + HHDM);
        break;
    default:
        break;
    }
    
    return UACPI_ITERATION_DECISION_CONTINUE;
}

struct acpi_madt_interrupt_source_override* find_int_src_override(uint8_t irq){
    for (uint32_t i = 0; i < int_src_overrides.size;i++){
        struct acpi_madt_interrupt_source_override* int_src_override = (struct acpi_madt_interrupt_source_override*)int_src_overrides.data[i];
        if (int_src_override->source == irq) return int_src_override;
    }

    return nullptr;
}

struct acpi_madt_ioapic* find_responsible_ioapic(uint32_t irq){
    for (uint32_t i = 0; i < ioapics.size; i++){
        struct acpi_madt_ioapic* ioapic = (struct acpi_madt_ioapic*)ioapics.data[i];
        if (irq >= ioapic->gsi_base && irq < ioapic->gsi_base + ioapic_get_redir_entry_cnt(ioapic->address + HHDM)){
            return ioapic;
        }
    }

    return nullptr;
}

void discover_ioapics(){
    init_vector(&ioapics);
    init_vector(&int_src_overrides);
    uacpi_table tbl;
    uacpi_status ret = uacpi_table_find_by_signature(ACPI_MADT_SIGNATURE,&tbl);
    if (uacpi_unlikely_error(ret))
        warn("Failed to find MADT");
    struct acpi_madt* madt = (struct acpi_madt*)tbl.hdr;
    logf("Local APIC address: %x",madt->local_interrupt_controller_address);
    
    lapic_data.apic_virt_base = (void*)(madt->local_interrupt_controller_address + HHDM);
    mem_map_page((uint64_t)lapic_data.apic_virt_base,madt->local_interrupt_controller_address,PAGE_FLAG_WRITE);

    ret = uacpi_for_each_subtable(tbl.hdr,sizeof(struct acpi_madt),madt_callback,NULL);
    if (uacpi_unlikely_error(ret))
        warn("Failed to parse MADT");
    
    uacpi_table_unref(&tbl);
}

uacpi_iteration_decision aml_root_bridge_callback(void* user, uacpi_namespace_node* node, uint32_t max_depth){
    
    uint64_t* data = (uint64_t*)user;
    uint64_t curr_bus = 0;

    // read Base Bus Number
    uacpi_status status = uacpi_eval_simple_integer(node,"_BBN",&curr_bus);
    if (uacpi_unlikely_error(status == UACPI_STATUS_NOT_FOUND)) 
        curr_bus = 0;
    
    if (curr_bus != data[0]) return UACPI_ITERATION_DECISION_CONTINUE;

    *((uint64_t*)data[1]) = (uint64_t)node; // save the root bridge namespace node
    return UACPI_ITERATION_DECISION_BREAK;

}

uint8_t ioapic_register_pci_irq(pci_device_t* dev, uint8_t int_pin){
    // find the correct pin on the I/O Apic, which the interrupt pin of the PCI bus connects to
    uacpi_namespace_node* root_bridge_namespace = nullptr;
    uint8_t ret = 0;
    uint64_t data[2] = {dev->bus,(uint64_t)&root_bridge_namespace};

    // parse AML with uACPI
    uacpi_find_devices("PNP0A03",aml_root_bridge_callback,(void*)data);

    if (!root_bridge_namespace) {
        errorf("No root bridge namespace for PIC device IRQ registering");
        return 0;
    }

    uacpi_pci_routing_table* PRT = 0;
    if (uacpi_get_pci_routing_table(root_bridge_namespace,&PRT) != UACPI_STATUS_OK){
        errorf("Failed to find PRT");
        return 0;
    }

    if (dev->bus != 0){
        // not on main bus, adjust for swizzeling
        int_pin = (dev->slot + int_pin) % 4;
    }

    uint8_t polarity = 1; // active low
    uint8_t trigger = 1; // level trigger
    uint32_t gsi = 0;
    for (uint32_t i = 0; i < PRT->num_entries;i++){
        uacpi_pci_routing_table_entry* entry = &PRT->entries[i];
        if (entry->pin != (int_pin - 1)) // INTA# (Int pin 1) == entry pin 0
            continue;
        
        uint16_t curr_func = (entry->address & 0xffff);
        uint16_t curr_slot = (entry->address >> 16) & 0xffff;

        if (curr_slot != dev->slot || (curr_func != dev->func && curr_func != 0xffff)){
            continue;
        }

        if (entry->source == 0){
            // direct bridge connection
            gsi = entry->index;
            break;
        }

        // try to get irq through ressources
        uacpi_resources* res = nullptr;
        if (uacpi_get_current_resources(entry->source, &res) != UACPI_STATUS_OK){
            errorf("Failed to fetch APIC resources");
            goto cleanup;
        }
        if (entry->index >= res->length){
            uacpi_free_resources(res);
            errorf("Overflow in APIC resources lookup");
            goto cleanup;
        }

        uacpi_resource* resource = &res->entries[entry->index];
        switch (resource->type)
        {
        case UACPI_RESOURCE_TYPE_IRQ:
            gsi = resource->irq.irqs[0];
            polarity = resource->irq.polarity == UACPI_POLARITY_ACTIVE_LOW;
            trigger = resource->irq.triggering == UACPI_TRIGGERING_LEVEL;
            break;
        case UACPI_RESOURCE_TYPE_EXTENDED_IRQ:
            gsi = resource->extended_irq.irqs[0];
            polarity = resource->extended_irq.polarity == UACPI_POLARITY_ACTIVE_LOW;
            trigger = resource->extended_irq.triggering == UACPI_TRIGGERING_LEVEL;
            break;
        default:
            panic("Unusable resource recieved in APIC IRQ registering");
            break;
        }
        uacpi_free_resources(res);
        break;

    }
    if (!gsi) {
        errorf("No gsi found for PCI IRQ");
        goto cleanup;
    }

    logf("PCI device [%d:%d.%d] registers on gsi %d",dev->bus,dev->slot,dev->func,gsi);
    struct acpi_madt_ioapic* ioapic = find_responsible_ioapic(gsi);
    if (!ioapic) {warnf("Found no responsible IO APIC for gsi %d",gsi); goto cleanup; }
    
    uint8_t pin = gsi - ioapic->gsi_base;
    uint8_t vec = APIC_INTERRUPT_START + pin;
    uint32_t val = vec;
    val |= (polarity & 0x1) << 13;
    val |= (trigger & 0x1) << 15;

    uint32_t dest =  lapic_data.apic_id << 24;

    write_ioapic_register(ioapic->address + HHDM,IOREDTBL(pin) + 0,val);
    write_ioapic_register(ioapic->address + HHDM,IOREDTBL(pin) + 1,dest);
    ret = vec;
cleanup:
    uacpi_free_pci_routing_table(PRT);
    return ret;
}

uint8_t ioapic_redirect_legacy_irq(uint32_t irq){
    struct acpi_madt_ioapic* ioapic = find_responsible_ioapic(irq);
    if (!ioapic) {warnf("Found no responsible IO APIC for IRQ %d",irq);return 0;}
    struct acpi_madt_interrupt_source_override* iso = find_int_src_override(irq);

    uint8_t polarity = 0; // active high
    uint8_t trigger = 0; // edge

    uint32_t gsi = irq; 
    uint32_t flags = 0; 

    if (iso) {
        gsi = iso->gsi;
        polarity = (iso->flags & 0x2) ? 1 : 0;
        trigger = (iso->flags & 0x8) ? 1 : 0; 
    }

    uint32_t pin = gsi - ioapic->gsi_base;
    uint8_t vec = APIC_INTERRUPT_START + pin;
    uint32_t val = vec;
    val |= (polarity & 0x1) << 13;
    val |= (trigger & 0x1) << 15;

    uint32_t dest =  lapic_data.apic_id << 24;

    write_ioapic_register(ioapic->address + HHDM,IOREDTBL(pin) + 0,val);
    write_ioapic_register(ioapic->address + HHDM,IOREDTBL(pin) + 1,dest);

    return vec;
}

void set_apic_base(){
    uint32_t low = ((uint64_t)lapic_data.apic_virt_base - HHDM) & 0xffff0000 | IA32_APIC_BASE_MSR_ENABLE;
    uint32_t high = (((uint64_t)lapic_data.apic_virt_base - HHDM) >> 32) & 0x0f;
    write_msr(IA32_APIC_BASE_MSR,low,high); 
}

void acpi_enable_apic(){
    set_apic_base();
    write_apic_register(APIC_REG_SPURIOUS_INT_VEC,read_apic_register(APIC_REG_SPURIOUS_INT_VEC) | 0x1ff); // set bit 8 of Spurious int vec to start recieving interrupts
}

void initialize_apics(){
    discover_ioapics();
    acpi_enable_apic();
}