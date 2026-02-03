#include "io_apic.h"
#include <uacpi/tables.h>
#include <uacpi/acpi.h>
#include "../memory/memory.h"
#include "../utilities/vector.h"
#include "../io/log.h"
#include <stdint.h>

vector_t ioapics;

void dump_ioapic_info(uintptr_t apic_base){
    uint8_t apicid = (read_ioapic_register(apic_base,IOAPICID) >> 24) & 0xf;
    logf("IO APIC Id: %d",apicid);
    uint8_t redir_entry_cnt = (read_ioapic_register(apic_base,IOAPICVER) >> 16) + 1;
    logf("This IO APIC supports up to %d redirections",redir_entry_cnt);
}

uacpi_iteration_decision madt_callback(void* user, struct acpi_entry_hdr* hdr){
    if (hdr->type != ACPI_MADT_ENTRY_TYPE_IOAPIC) return UACPI_ITERATION_DECISION_CONTINUE;
    
    // found an IO APIC
    struct acpi_madt_ioapic* ioapic = (struct acpi_madt_ioapic*)hdr;
    logf("Found IO APIC at address %x",ioapic->address); // base for MMIO
    mem_map_page(ioapic->address + HHDM,ioapic->address,PAGE_FLAG_WRITE);

    vector_append(&ioapics,(vector_data_t)ioapic);
    dump_ioapic_info(ioapic->address + HHDM);
    return UACPI_ITERATION_DECISION_BREAK;
}

void discover_ioapics(){
    init_vector(&ioapics);
    uacpi_table tbl;
    uacpi_status ret = uacpi_table_find_by_signature(ACPI_MADT_SIGNATURE,&tbl);
    if (uacpi_unlikely_error(ret))
        warn("Failed to find MADT");
    ret = uacpi_for_each_subtable(tbl.hdr,sizeof(struct acpi_madt),madt_callback,NULL);
    if (uacpi_unlikely_error(ret))
        warn("Failed to parse MADT");
    
    uacpi_table_unref(&tbl);
}

void write_ioapic_register(uintptr_t apic_base,uint8_t offset, uint32_t val){
    // select register via IOREGSEL
    *(volatile uint32_t*)(apic_base) = offset;
    // write to IOWIN
    *(volatile uint32_t*)(apic_base + 0x10) = val;
}

uint32_t read_ioapic_register(uintptr_t apic_base,uint8_t offset){
    *(volatile uint32_t*)(apic_base) = offset;

    return *(volatile uint32_t*)(apic_base + 0x10);
}
