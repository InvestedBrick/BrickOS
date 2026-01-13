#include "pci.h"
#include "../../io/io.h"
#include "../../io/log.h"
#include "../../utilities/util.h"
#include "../../memory/kmalloc.h"
pci_device_t* head = nullptr;
void check_device(uint8_t bus, uint8_t dev);

uint16_t pci_config_read_word(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset){
    uint32_t lbus  = (uint32_t)bus;
    uint32_t ldev = (uint32_t)dev;
    uint32_t lfunc = (uint32_t)func;
    
    uint32_t config_addr = (uint32_t)((lbus << 16) | (ldev << 11)  |
                                      (lfunc << 8) | (offset & 0xfc) |
                                      (uint32_t)(1 << 31) );
    outl(PCI_CONFIG_ADDRESS,config_addr);

    uint16_t data = (uint16_t)((inl(PCI_CONFIG_DATA) >> ((offset & 0x2) ? 16 : 0)) & 0xffff);

    return data;
}

void pci_config_write_dword(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset,uint32_t config){
    uint32_t config_addr = (uint32_t)((bus << 16) | (dev << 11)  |
                                      (func << 8) | (offset & 0xfc) |
                                      (uint32_t)(1 << 31) );
    outl(PCI_CONFIG_ADDRESS,config_addr);
    outl(PCI_CONFIG_DATA,config);
}


uint16_t get_vendor_id(uint8_t bus, uint8_t dev, uint8_t func){
    return pci_config_read_word(bus,dev,func,0x0);
}
uint16_t get_device_id(uint8_t bus, uint8_t dev, uint8_t func){
    return pci_config_read_word(bus, dev, func, 0x2);
}
uint8_t get_header_type(uint8_t bus,uint8_t dev, uint8_t func){
    return (uint8_t)(pci_config_read_word(bus,dev,func,0xe) & 0xff);
}
uint8_t get_class_code(uint8_t bus, uint8_t dev, uint8_t func){
    return (uint8_t)((pci_config_read_word(bus,dev,func,0xa) >> 0x8) & 0xff);
}
uint8_t get_subclass(uint8_t bus,uint8_t dev, uint8_t func){
    return (uint8_t)(pci_config_read_word(bus,dev,func,0xa) & 0xff);
}
uint8_t get_secondary_bus(uint8_t bus, uint8_t dev, uint8_t func){
    return (uint8_t)((pci_config_read_word(bus,dev,func,0x18) >> 0x8) & 0xff);
}

void add_pci_device(uint8_t bus, uint8_t dev, uint8_t func){
    
    pci_device_t* device = kmalloc(sizeof(pci_device_t));
    device->next = nullptr;
    device->bus = bus;
    device->dev = dev;
    device->func = func;
    device->device_id = get_device_id(bus,dev,func);
    device->header_type = get_header_type(bus,dev,func);
    device->class_code = get_class_code(bus,dev,func);
    device->subclass = get_subclass(bus,dev,func);
    device->vendor_id = get_vendor_id(bus,dev,func);
    logf("Found PCI Device - Bus: %x, Dev: %x, Func: %x, Vendor ID: %x, Device ID: %x, Class: %x, Subclass: %x",
        bus,dev,func,device->vendor_id,device->device_id,device->class_code,device->subclass);

    if (!head) head = device;
    else {
        pci_device_t* it = head;
        while(it->next) it = it->next;
        it->next = device;
    }
}

void check_bus(uint8_t bus){
    for (uint8_t dev = 0; dev < 32; dev++){
        check_device(bus,dev);
    }
}

void check_function(uint8_t bus, uint8_t dev, uint8_t func){
    uint8_t class = get_class_code(bus,dev,func);
    uint8_t sub_class = get_subclass(bus,dev,func);
    if (class == PCI_CLASS_BRIDGE && sub_class == PCI_SUBCLASS_PCI_TO_PCI_BRIDGE){
        uint8_t secondary_bus = get_secondary_bus(bus,dev,func);
        check_bus(secondary_bus);
    }
}

void check_device(uint8_t bus, uint8_t dev){
    uint8_t function = 0;
    uint16_t vendor_id = get_vendor_id(bus,dev,function);
    if (vendor_id == PCI_DEV_DOESNT_EXIST_VENDOR_ID) return;
    check_function(bus,dev,function);
    add_pci_device(bus,dev,function);
    uint8_t header_type = get_header_type(bus,dev,function);
    if ((header_type & PCI_HEADER_FLAG_MF) != 0){
        //multifunc dev
        for (function = 1; function < 8; function++){
            if (get_vendor_id(bus,dev,function) != PCI_DEV_DOESNT_EXIST_VENDOR_ID){
                check_function(bus,dev,function);
                add_pci_device(bus,dev,function);
            }
        }
    }
}

void pci_check_all_busses(){
    uint8_t header_type = get_header_type(0,0,0);
    uint8_t bus; 
    uint8_t func;
    if ((header_type & PCI_HEADER_FLAG_MF) == 0){
        check_bus(0);
    }else{
        for (func = 0; func < 8; func++){
            if (get_vendor_id(0,0,func) == PCI_DEV_DOESNT_EXIST_VENDOR_ID) break;
            bus = func;
            check_bus(bus);
        }
    }
}