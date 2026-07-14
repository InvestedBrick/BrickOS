#include "82540EM.h"
#include "../../io/log.h"
#include "../../io/io.h"
#include "../../memory/kmalloc.h"
#include "../../memory/memory.h"
#include "../../utilities/util.h"
#include "../PCI/pci.h"
#include "../../tables/syscalls.h"
#include "../../ACPI/acpi.h"
#include "../../ACPI/apic.h"
#include "../../networking/ethernet.h"
#include "../../networking/networking.h"
#include "../../processes/scheduler.h"
#include <uacpi/uacpi.h>
#include <uacpi/utilities.h>

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

uint64_t map_82540em_BAR_memory_space(pci_device_t* dev, uint16_t* config_off){
    uint32_t bar = pci_config_read_dword(dev->bus,dev->slot,dev->func,*config_off);
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
        uint32_t bar0_high = pci_config_read_dword(dev->bus,dev->slot,dev->func,*config_off);
        *config_off += 4;

        phys_base_addr |= ((uint64_t)bar0_high << 32); 
    }

    uint32_t size = pci_get_base_addr_reg_space(dev->bus,dev->slot,dev->func,bar_idx);
    
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
    ctrl &= ~I8254x_CTRL_VME;
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

void i8254x_recv_packets(i82540em_t* nic){

    while(nic->rx_ring[nic->rx_next].status & I8254x_RX_STAT_DD){
        i8254x_rx_descriptor_t* rx_desc = &nic->rx_ring[nic->rx_next];
        if (!nic->accumulating){
            nic->start_idx = nic->rx_next;
            nic->total_size = 0;
            nic->accumulating = 1;
        }

        nic->total_size += rx_desc->len;
        if (rx_desc->status & I8254x_RX_STAT_EOP){
            void* data_buffer = kmalloc(nic->total_size);
            uint32_t write_off = 0;
            uint32_t idx = nic->start_idx;
            while (1){
                i8254x_rx_descriptor_t* desc = &nic->rx_ring[idx];
                uint64_t buff_virt = map_somewhere_rw(desc->buff_addr);
                memcpy((void*)((uint64_t)data_buffer + write_off),(void*)buff_virt,desc->len);
                mem_unmap_page(buff_virt);
                write_off += desc->len;
                uint8_t status = desc->status;
                desc->status = 0;
                idx = (idx + 1) % I8254x_N_RX_DESCRS;
                if (status & I8254x_RX_STAT_EOP) break;
            }
            nic->accumulating = 0;
            nic->total_size = 0;
            ethernet_handle_packet(data_buffer,write_off);
        }

        nic->rx_next = (nic->rx_next + 1) % I8254x_N_RX_DESCRS;
        
    }

    uint32_t tail = (nic->rx_next == 0) ? I8254x_N_RX_DESCRS - 1 : nic->rx_next - 1;
    i82540em_mmio_reg_write(nic,I8254x_REG_RDT,tail);
}

void i8254x_send_packet(i82540em_t* nic, void* data, uint64_t len,bool EOP){
    uint32_t tx_tail = i82540em_mmio_reg_read(nic,I8254x_REG_TDT);
    i8254x_tx_descriptor_t* tx_desc = &nic->tx_ring[tx_tail];

    while(!(tx_desc->status & I8254x_TX_STAT_DD)){
        invoke_scheduler();
    }

    // allocate buffer (cleaned up in int handler)
    uint64_t buff_phys = pmm_alloc_page_frame();
    uint64_t buff_virt = map_somewhere_rw(buff_phys);
    memcpy((void*)buff_virt,data,len);
    mem_unmap_page(buff_virt);
    tx_desc->buff_addr = buff_phys;
    tx_desc->len = len;
    tx_desc->status = 0;

    tx_desc->cmd = I8254x_TX_CMD_RS;
    if (EOP) tx_desc->cmd |= I8254x_TX_CMD_EOP | I8254x_TX_CMD_IFCS;


    tx_tail = (tx_tail + 1) % I8254x_N_TX_DESCRS;
    i82540em_mmio_reg_write(nic,I8254x_REG_TDT,tx_tail);
}

uint32_t i8254x_send(void* data, uint32_t len){
    uint64_t sent = 0;
    while (sent < len){
        uint64_t to_send = min(len - sent, MEMORY_PAGE_SIZE);
        i8254x_send_packet(i82540em,(void*)((uint64_t)data + sent),to_send,to_send == (len - sent));
        sent += to_send;
    }
    return sent;
}

void i8254x_interrupt_handler(interrupt_stack_frame_t* frame){
    uint32_t icr = i82540em_mmio_reg_read(i82540em,I8254x_REG_ICR);
    if (icr & I8254x_IMS_RXT0){
        //packet recieved
        i8254x_recv_packets(i82540em);
    }

    if (icr & I8254x_IMS_LSC){
        //packet recieved
        log("82540EM Link Status interrupt");
        if (i82540em_mmio_reg_read(i82540em,I8254x_REG_STATUS) & I8254x_STATUS_LU){
            log("Link is up");
        } else {
            log("Link is down");
        }
    }

    // cleanup TX buffers
    while (i82540em->tx_cleanup != i82540em_mmio_reg_read(i82540em,I8254x_REG_TDT)){
        i8254x_tx_descriptor_t* tx_desc = &i82540em->tx_ring[i82540em->tx_cleanup];
        if (!(tx_desc->status & I8254x_TX_STAT_DD)) break;

        pmm_free_page_frame(tx_desc->buff_addr);
        tx_desc->status = 0;
        tx_desc->cmd = 0;
        i82540em->tx_cleanup = (i82540em->tx_cleanup + 1) % I8254x_N_TX_DESCRS;
    }

}
void i82540em_rx_setup(i82540em_t* nic){
    uint32_t rx_ring_sz = I8254x_N_RX_DESCRS * sizeof(i8254x_rx_descriptor_t);
    if (rx_ring_sz > MEMORY_PAGE_SIZE) {
        errorf("RX ring too large; TODO: implement multi-page phys allocator");
        return;
    }

    uint64_t rx_ring_phys = pmm_alloc_page_frame();
    nic->rx_ring = (i8254x_rx_descriptor_t*)map_somewhere_rw(rx_ring_phys);
    for (uint32_t i = 0; i < I8254x_N_RX_DESCRS;i++){
        uint64_t buff_phys = pmm_alloc_page_frame();
        nic->rx_ring[i].buff_addr = buff_phys;
    }

    i82540em_mmio_reg_write(nic,I8254x_REG_RDBAL,(uint32_t)(rx_ring_phys & 0xffffffff));
    i82540em_mmio_reg_write(nic,I8254x_REG_RDBAH,(uint32_t)(rx_ring_phys >> 32));
    i82540em_mmio_reg_write(nic,I8254x_REG_RDLEN,rx_ring_sz);
    i82540em_mmio_reg_write(nic,I8254x_REG_RDH,0);
    i82540em_mmio_reg_write(nic,I8254x_REG_RDT,I8254x_N_RX_DESCRS - 1);
    nic->rx_next = 0;
    nic->accumulating = 0;
    nic->total_size = 0;
    nic->start_idx = 0;

    uint32_t rx_ctrl = I8254x_RCTL_EN | I8254x_RCTL_LPE | I8254x_RCTL_UPE | I8254x_RCTL_MPE | I8254x_RCTL_BAM | I8254x_RCTL_BSEX | I8254x_RCTL_BSIZE_4096;
    i82540em_mmio_reg_write(nic,I8254x_REG_RCTL,rx_ctrl);
}

void i82540em_tx_setup(i82540em_t* nic){
    uint32_t tx_ring_sz = I8254x_N_TX_DESCRS * sizeof(i8254x_tx_descriptor_t);
    if (tx_ring_sz > MEMORY_PAGE_SIZE) {
        errorf("TX ring too large; TODO: implement multi-page phys allocator");
        return;
    }
    uint64_t tx_ring_phys = pmm_alloc_page_frame();
    nic->tx_ring = (i8254x_tx_descriptor_t*)map_somewhere_rw(tx_ring_phys);
    memset((void*)nic->tx_ring,0x0,tx_ring_sz);

    for (uint32_t i = 0; i < I8254x_N_TX_DESCRS;i++){
        nic->tx_ring[i].status |= I8254x_TX_STAT_DD;
    }

    i82540em_mmio_reg_write(nic,I8254x_REG_TDBAL,(uint32_t)(tx_ring_phys & 0xffffffff));
    i82540em_mmio_reg_write(nic,I8254x_REG_TDBAH,(uint32_t)(tx_ring_phys >> 32));
    i82540em_mmio_reg_write(nic,I8254x_REG_TDLEN,tx_ring_sz);
    i82540em_mmio_reg_write(nic,I8254x_REG_TDH,0);
    i82540em_mmio_reg_write(nic,I8254x_REG_TDT,0);
    nic->tx_cleanup = 0;
    
    uint32_t tx_ctrl = I8254x_TCTL_EN |  I8254x_TCTL_PSP;
    i82540em_mmio_reg_write(nic,I8254x_REG_TCTL,tx_ctrl);


}

void i8254x_enable_interrupts(i82540em_t* nic){
    uint32_t ims = I8254x_IMS_RXT0 | I8254x_IMS_RXO | I8254x_IMS_LSC;
    i82540em_mmio_reg_write(nic,I8254x_REG_IMS,ims);
}

void i8254x_disable_interrupts(i82540em_t* nic){
    i82540em_mmio_reg_write(nic,I8254x_REG_IMS,0);
}

void init_82540EM_driver(net_interface_t* iface, pci_device_t* dev){
    log("FOUND THE 82540EM");

    iface->mtu = DEFAULT_MTU;
    i82540em = (i82540em_t*)kmalloc(sizeof(i82540em_t));
    iface->send = i8254x_send;

    uint16_t cmd = pci_config_read_word(dev->bus,dev->slot,dev->func,0x4);
    cmd |= 0x7; // enable I/O, memspace and bus master
    pci_dev_write_command(dev,cmd);

    uint16_t curr_config_off = 0x10; // BAR0 offset

    // Memory register base address
    i82540em->reg_base_addr = map_82540em_BAR_memory_space(dev,&curr_config_off);


    i82540em_enable_eeprom(i82540em);
    i82540em_reset(i82540em,iface->mac_addr);

    log_MAC(iface->mac_addr);
    
    //NOTE: Qemu does not seem to support emulating flash memory and just moves the I/O base addr up
    uint32_t bar = pci_config_read_dword(dev->bus,dev->slot,dev->func,curr_config_off);
    if (bar & 0x1){
        i82540em->io_reg_base_addr = bar & ~0x1;
    }

    i82540em_tx_setup(i82540em);
    i82540em_rx_setup(i82540em);
    
    i8254x_enable_interrupts(i82540em);

    uint8_t int_pin = (pci_config_read_word(dev->bus,dev->slot,dev->func,0x3c) >> 8) & 0xff;
    uint8_t irq = ioapic_register_pci_irq(dev,int_pin);
    register_irq(irq,i8254x_interrupt_handler);

}