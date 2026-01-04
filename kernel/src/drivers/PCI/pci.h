#ifndef INCLUDE_PCI_H
#define INCLUDE_PCI_H

#define PCI_CONFIG_ADDRESS 0xcf8
#define PCI_DATA 0xcfc

#include <stdint.h>

uint16_t pci_config_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

#endif