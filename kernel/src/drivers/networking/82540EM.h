#ifndef INCLUDE_82540EM_H
#define INCLUDE_82540EM_H
#include "../PCI/pci.h"

typedef struct{
    pci_device_t* dev;

    uint64_t reg_base_addr;
    uint64_t flash_base_addr;
    uint64_t io_reg_base_addr;

    uint8_t reg_base_addr_size; // 32 or 64
    uint8_t flash_base_addr_size; //  
}i82540em_t;
#define DEV_REG_CTRL_8254x      0x00000
#define DEV_REG_STATUS_8254x    0x00008
#define DEV_REG_EECD_8254x      0x00010
#define DEV_REG_EERD_8254x      0x00014

#define DEV_REG_ICR_8254x       0x000C0
#define DEV_REG_IMS_8254x       0x000D0

#define DEV_REG_RCTL_8254x      0x00100

#define DEV_REG_RDBAL_8254x     0x02800
#define DEV_REG_RDBAH_8254x     0x02804
#define DEV_REG_RDLEN_8254x     0x02808
#define DEV_REG_RDH_8254x       0x02810
#define DEV_REG_RDT_8254x       0x02818

#define DEV_REG_TCTL_8254x      0x00400

#define DEV_REG_TDBAL_8254x     0x03800
#define DEV_REG_TDBAH_8254x     0x03804
#define DEV_REG_TDLEN_8254x     0x03808
#define DEV_REG_TDH_8254x       0x03810
#define DEV_REG_TDT_8254x       0x03818

/* Receive Address Registers (8-byte stride) */
#define DEV_REG_RAL_8254x(n)    (0x05400 + ((n) * 8))
#define DEV_REG_RAH_8254x(n)    (0x05404 + ((n) * 8))

void init_82540EM_driver(pci_device_t* dev);
#endif