#include "82540EM.h"
#include "../../io/log.h"
#include "../../io/io.h"
#include "../../memory/kmalloc.h"
#include "../../memory/memory.h"
#include "../../utilities/util.h"
#include "../PCI/pci.h"
i82540em_t* i82540em = nullptr;

void i82540em_mmio_reg_write(i82540em_t* dev, uint16_t reg,uint32_t val){
    *(uint32_t*)(dev->reg_base_addr + reg) = val;
}

uint32_t i82540em_mmio_reg_read(i82540em_t* dev, uint16_t reg){
    return *(uint32_t*)(dev->reg_base_addr + reg);
}

void i82540em_io_reg_write(i82540em_t* dev, uint16_t reg,uint32_t val){
    outl(dev->io_reg_base_addr + 0x0, reg); // set IOADDR
    outl(dev->io_reg_base_addr + 0x4,val); 
}

uint32_t i82540em_io_reg_read(i82540em_t* dev, uint16_t reg){
    outl(dev->io_reg_base_addr + 0x0, reg); // set IOADDR
    return inl(dev->io_reg_base_addr + 0x4);
}
void init_82540EM_driver(pci_device_t* dev){
    log("FOUND THE 82540EM");
    i82540em = (i82540em_t*)kmalloc(sizeof(i82540em_t));
    i82540em->dev = dev;

    uint16_t curr_config_off = 0x10; // BAR0 offset

    uint32_t bar0 = pci_config_read_dword(dev->bus,dev->dev,dev->func,curr_config_off);
    curr_config_off += 0x4;
    if (bar0 & 0x1){
        error("i82540em BAR0 detected as MMIO BAR");
    }

    // Register Base Addr
    uint16_t reg_base_addr_size = (bar0 & 0x4) ? 64 : 32;
    uint64_t reg_phys_base_addr = bar0 & 0xfffffff0;
    if (reg_base_addr_size == 64){
        uint32_t bar0_high = pci_config_read_dword(dev->bus,dev->dev,dev->func,curr_config_off);
        curr_config_off += 4;

        reg_phys_base_addr|= (bar0_high << 32); 
    }

    logf("i82540em register base addr: %x",i82540em->reg_base_addr);
    uint32_t size = pci_get_base_addr_reg_space(dev->bus,dev->dev,dev->func,0);
    
    //offset map into higher half
    uint32_t n_pages = size / MEMORY_PAGE_SIZE;
    for (uint32_t i = 0; i < n_pages;i++){
        mem_map_page(reg_phys_base_addr+ i * MEMORY_PAGE_SIZE + HHDM,
                     reg_phys_base_addr+ i * MEMORY_PAGE_SIZE,
                    PAGE_FLAG_WRITE
        );

    }
    i82540em->reg_base_addr = reg_phys_base_addr + HHDM; 

}