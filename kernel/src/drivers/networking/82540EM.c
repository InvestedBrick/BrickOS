#include "82540EM.h"
#include "../../io/log.h"
#include "../../io/io.h"
#include "../../memory/kmalloc.h"
#include "../../memory/memory.h"
#include "../../utilities/util.h"
#include "../PCI/pci.h"
#include "../../tables/syscalls.h"
i82540em_t* i82540em = nullptr;

void i82540em_mmio_reg_write(i82540em_t* nic, uint16_t reg,uint32_t val){
    *(uint32_t*)(nic->reg_base_addr + reg) = val;
}

uint32_t i82540em_mmio_reg_read(i82540em_t* nic, uint16_t reg){
    return *(uint32_t*)(nic->reg_base_addr + reg);
}

void i82540em_io_reg_write(i82540em_t* nic, uint16_t reg,uint32_t val){
    outl(nic->io_reg_base_addr + 0x0, reg); // set IOADDR
    outl(nic->io_reg_base_addr + 0x4,val); 
}

uint32_t i82540em_io_reg_read(i82540em_t* nic, uint16_t reg){
    outl(nic->io_reg_base_addr + 0x0, reg); // set IOADDR
    return inl(nic->io_reg_base_addr + 0x4);
}

uint64_t map_82540em_BAR_memory_space(i82540em_t* nic, uint16_t* config_off){
    pci_device_t* dev = nic->dev;
    uint32_t bar = pci_config_read_dword(dev->bus,dev->dev,dev->func,*config_off);
    logf("Bar: %x",bar);
    uint8_t bar_idx = (*config_off - 0x10) / 0x4;
    
    *config_off += 0x4;
    if (bar & 0x1){
        error("i82540em MMIO BAR detected as I/O BAR");
        return 0;
    }

    bool is64 = (bar & 0x4);
    uint64_t phys_base_addr = bar & 0xfffffff0;
    if (is64){
        logf("64 BIT ALERT");
        uint32_t bar0_high = pci_config_read_dword(dev->bus,dev->dev,dev->func,*config_off);
        *config_off += 4;

        phys_base_addr |= ((uint64_t)bar0_high << 32); 
    }

    uint32_t size = pci_get_base_addr_reg_space(dev->bus,dev->dev,dev->func,bar_idx);
    
    //offset map into higher half
    uint32_t n_pages = size / MEMORY_PAGE_SIZE;
    for (uint32_t i = 0; i < n_pages;i++){
        mem_map_page(phys_base_addr+ i * MEMORY_PAGE_SIZE + HHDM,
                     phys_base_addr+ i * MEMORY_PAGE_SIZE,
                    PAGE_FLAG_WRITE
        );

    }


    return phys_base_addr + HHDM;

}

void i82540em_enable_eeprom(i82540em_t* nic){
    uint32_t eecd = i82540em_mmio_reg_read(nic,I8254x_REG_EECD);
    eecd |= I8254x_EECD_SK | I8254x_EECD_CS | I8254x_EECD_DI;
    i82540em_mmio_reg_write(nic,I8254x_REG_EECD,eecd);
}


void i82540em_eeprom_aquire_lock(i82540em_t* nic){
    uint32_t eecd = i82540em_mmio_reg_read(nic,I8254x_REG_EECD);
    eecd |= I8254x_EECD_EE_REQ;
    i82540em_mmio_reg_write(nic,I8254x_REG_EECD,eecd);

    while(!(i82540em_mmio_reg_read(nic,I8254x_REG_EECD) & I8254x_EECD_EE_GNT)){
        asm ("nop");
    }
}

void i82540em_eeprom_free_lock(i82540em_t* nic){
    uint32_t eecd = i82540em_mmio_reg_read(nic,I8254x_REG_EECD);
    eecd &= ~(uint32_t)I8254x_EECD_EE_REQ;
    i82540em_mmio_reg_write(nic,I8254x_REG_EECD,eecd);
}

uint16_t i82540em_eeprom_read(i82540em_t* nic,uint8_t addr){

    uint32_t tmp;
    uint16_t data;

    if (!(i82540em_mmio_reg_read(nic,I8254x_REG_EECD) & I8254x_EECD_EE_PRES)){
        error("i82540em EEPROM present bit not set");
        return 0;
    }
    i82540em_eeprom_aquire_lock(nic);
    tmp = ((uint32_t)addr & 0xff) << 8;
    tmp |= I8254x_EERD_START;
    i82540em_mmio_reg_write(nic,I8254x_REG_EERD,tmp);

    while(!(i82540em_mmio_reg_read(nic,I8254x_REG_EERD) & I8254x_EERD_DONE)){
        asm ("nop");
    }
    data = (uint16_t)(i82540em_mmio_reg_read(nic,I8254x_REG_EERD) >> 16 & 0xffff);
    tmp = i82540em_mmio_reg_read(nic,I8254x_REG_EERD);
    tmp &= ~(uint32_t)I8254x_EERD_START;
    i82540em_mmio_reg_write(nic,I8254x_REG_EERD,tmp);

    i82540em_eeprom_free_lock(nic);

    return data;
}

void i82540em_reset(i82540em_t* nic, uint8_t* mac_addr){
    uint32_t ctrl = i82540em_mmio_reg_read(nic,I8254x_REG_CTRL);
    ctrl |= I8254x_CTRL_RESET;
    i82540em_mmio_reg_write(nic,I8254x_REG_CTRL,ctrl);

    // wait for self-clear
    while (i82540em_mmio_reg_read(nic,I8254x_REG_CTRL) & I8254x_CTRL_RESET){
        asm ("nop");
    }
    
    ctrl = i82540em_mmio_reg_read(nic,I8254x_REG_CTRL);
    ctrl |= I8254x_CTRL_ASDE | I8254x_CTRL_SLU; // auto speed detection and set Link Up

    i82540em_mmio_reg_write(nic,I8254x_REG_CTRL,ctrl);

    uint16_t b0 = i82540em_eeprom_read(nic,0);
    uint16_t b1 = i82540em_eeprom_read(nic,1);
    uint16_t b2 = i82540em_eeprom_read(nic,2);

    mac_addr[0] = b0 & 0xFF;
    mac_addr[1] = b0 >> 8;
    mac_addr[2] = b1 & 0xFF;
    mac_addr[3] = b1 >> 8;
    mac_addr[4] = b2 & 0xFF;
    mac_addr[5] = b2 >> 8;
    
    uint32_t ral0 = ((uint32_t)b1 << 16) | b0;
    uint32_t rah0 = (uint32_t)b2;

    i82540em_mmio_reg_write(nic,I8254x_REG_RAL(0),ral0);
    i82540em_mmio_reg_write(nic,I8254x_REG_RAH(0),rah0);

}

void log_MAC(uint8_t* mac_addr){
    unsigned char* mac_str = (unsigned char*)kmalloc(18);
    mac_str[17] = 0;
    char* hex = "0123456789abcdef";
    for (uint32_t i = 0; i < 6;i++){
        uint8_t byte = mac_addr[i];
        mac_str[i * 3] = hex[byte / 16];
        mac_str[i * 3 + 1] = hex[byte % 16];
        if (i < 5)
            mac_str[i * 3 + 2] = ':';
    }

    logf("MAC ADDR: %s",mac_str);
    kfree(mac_str);
}

void init_82540EM_driver(pci_device_t* dev){
    log("FOUND THE 82540EM");
    i82540em = (i82540em_t*)kmalloc(sizeof(i82540em_t));
    i82540em->dev = dev;

    uint16_t cmd = pci_config_read_word(dev->bus,dev->dev,dev->func,0x4);
    cmd |= 0x7; // enable I/O, memspace and bus master
    pci_dev_write_command(dev,cmd);

    uint16_t curr_config_off = 0x10; // BAR0 offset

    // Memory register base address
    i82540em->reg_base_addr = map_82540em_BAR_memory_space(i82540em,&curr_config_off);
    logf("i82540em reg base addr: %x",i82540em->reg_base_addr);


    uint8_t mac_addr[6] = {0};
    i82540em_enable_eeprom(i82540em);
    i82540em_reset(i82540em,mac_addr);

    log_MAC(mac_addr);
    
    //NOTE: Qemu does not seem to support emulating flash memory and just moves the I/O base addr up
    uint32_t bar = pci_config_read_dword(dev->bus,dev->dev,dev->func,curr_config_off);
    if (bar & 0x1){
        i82540em->io_reg_base_addr = bar & ~0x1;
    }

    uint16_t int_stuff = pci_config_read_word(dev->bus,dev->dev,dev->func,0x3c);
    uint8_t int_line = (uint8_t)(int_stuff & 0xff);
    uint8_t int_pin = (uint8_t)((int_stuff >> 8) & 0xff);
    logf("int line: %x, int pin: %x",int_line,int_pin);

}