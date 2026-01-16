#ifndef INCLUDE_PCI_H
#define INCLUDE_PCI_H

#define PCI_CONFIG_ADDRESS 0xcf8
#define PCI_CONFIG_DATA 0xcfc 
#define PCI_DEV_DOESNT_EXIST_VENDOR_ID 0xffff
#define PCI_HEADER_FLAG_MF (1 << 0x7)
#define PCI_CLASS_BRIDGE 0x6
#define PCI_SUBCLASS_PCI_TO_PCI_BRIDGE 0x4 
#include <stdint.h>

typedef struct pci_device {
    struct pci_device* next;

    uint8_t bus;
    uint8_t dev;
    uint8_t func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t header_type;
} pci_device_t;

/**
 * pci_config_read_word:
 * reads a word from the configuration space of a pci device at a specified offset
 * @param bus The bus
 * @param dev The device slot
 * @param func The device function
 * @param offset The offset into the configuration space
 * 
 * @return The read word
 */
uint16_t pci_config_read_word(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);

/**
 * pci_config_write_dword:
 * writes a dword to the configuration space if a pci device at a specified offset
 * @param bus The bus
 * @param dev The device slot
 * @param func The device function
 * @param offset The offset into the configuration space
 * @param config The value to be written
 */
void pci_config_write_dword(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset,uint32_t config);

void pci_check_all_busses();
#endif