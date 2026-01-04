#include "pci.h"
#include "../../io/io.h"
uint16_t pci_config_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset){
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    
    uint32_t config_addr = (uint32_t)((lbus << 16) | (lslot << 11)  |
                                      (lfunc << 8) | (offset & 0xfc) |
                                      (uint32_t)(1 << 31) );
    outl(PCI_CONFIG_ADDRESS,config_addr);

    uint16_t data = (uint16_t)((inl(PCI_DATA) >> ((offset & 0x2) ? 16 : 0)) & 0xffff);

    return data;
}
