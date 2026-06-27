#ifndef INCLUDE_PCI_H
#define INCLUDE_PCI_H

#define PCI_CONFIG_ADDRESS 0xcf8
#define PCI_CONFIG_DATA 0xcfc 
#define PCI_INTEL_VENDOR_ID 0x8086
#define PCI_DEV_DOESNT_EXIST_VENDOR_ID 0xffff
#define PCI_HEADER_FLAG_MF (1 << 0x7)

#define PCI_CLASS_MASS_STORAGE_CONTROLLER 0x1

#define PCI_CLASS_NETWORK_CONTROLLER 0x2
#define PCI_SUBCLASS_ETHERNET_CONTROLLER 0x0

#define PCI_CLASS_DISPLAY_CONTROLLER 0x3

#define PCI_CLASS_MULTIMEDIA_CONTROLLER 0x4

#define PCI_CLASS_MEMORY_CONTROLLER 0x5

#define PCI_CLASS_BRIDGE 0x6
#define PCI_SUBCLASS_PCI_TO_PCI_BRIDGE 0x4 

#define PCI_CLASS_SIMPLE_COM_CONTROLLER 0x7

#define PCI_CLASS_BASE_SYSTEM_PERIPHERAL 0x8

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

extern pci_device_t* pci_head;

/**
 * pci_config_read_word:
 * reads a word from the configuration space of a pci device at a specified offset
 * @param bus The bus
 * @param dev The device slot
 * @param func The device function
 * @param offset The offset into the configuration space (must be 2 byte aligned)
 * 
 * @return The read word
 */
uint16_t pci_config_read_word(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);

/**
 * pci_config_read_dword:
 * reads a dword from the configuration space of a pci device at a specified offset
 * @param bus The bus
 * @param dev The device slot
 * @param func The device function
 * @param offset The offset into the configuration space (must be 2 byte aligned)
 * 
 * @return The read dword
 */
uint32_t pci_config_read_dword(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);

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

/**
 * pci_dev_read_status:
 * Reads and returns the status register of a given PCI device
 * @param dev The device
 * @return the status register content
 */
uint16_t pci_dev_read_status(pci_device_t* dev);

/**
 * pci_dev_write_command:
 * Writes to the command register of a given PCI device
 * @param dev The device
 * @param data The data to write
 */
void pci_dev_write_command(pci_device_t* dev,uint16_t data);


/**
 * pci_get_base_addr_reg_space:
 * Returns the size of a MMIO address space defined by a given BAR
 * @param bus The bus
 * @param dev The device slot
 * @param func The device function
 * @param bar The Base Address Register ( 0 => BAR0, ...)
 * @return The size of the address space in bytes
 */
uint32_t pci_get_base_addr_reg_space(uint8_t bus, uint8_t dev, uint8_t func, uint8_t bar);
#endif