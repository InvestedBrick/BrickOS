#ifndef INCLUDE_PCI_H
#define INCLUDE_PCI_H

#define PCI_CONFIG_ADDRESS 0xcf8
#define PCI_DATA 0xcfc
#define PCI_DEV_DOESNT_EXIST_VENDOR_ID 0xffff
#define PCI_HEADER_FLAG_MF (1 << 0x7)
#define PCI_CLASS_BRIDGE 0x6
#define PCI_SUBCLASS_PCI_TO_PCI_BRIDGE 0x4 
#define PCI_MAX_DEVICES 256
#include <stdint.h>

typedef struct pci_device {
    uint8_t bus;
    uint8_t dev;
    uint8_t func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t header_type;
} pci_device_t;

uint16_t pci_config_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);


void pci_check_all_busses();
#endif