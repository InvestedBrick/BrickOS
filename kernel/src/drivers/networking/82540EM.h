#ifndef INCLUDE_82540EM_H
#define INCLUDE_82540EM_H
#include "../PCI/pci.h"

typedef struct{
    pci_device_t* dev;

    uint64_t reg_base_addr;
    uint64_t flash_base_addr;
    uint64_t io_reg_base_addr;

}i82540em_t;
#define I8254x_REG_CTRL      0x00000
#define I8254x_REG_STATUS    0x00008
#define I8254x_REG_EECD      0x00010
#define I8254x_REG_EERD      0x00014

#define I8254x_REG_ICR       0x000C0
#define I8254x_REG_IMS       0x000D0

#define I8254x_REG_RCTL      0x00100

#define I8254x_REG_RDBAL     0x02800
#define I8254x_REG_RDBAH     0x02804
#define I8254x_REG_RDLEN     0x02808
#define I8254x_REG_RDH       0x02810
#define I8254x_REG_RDT       0x02818

#define I8254x_REG_TCTL      0x00400

#define I8254x_REG_TDBAL     0x03800
#define I8254x_REG_TDBAH     0x03804
#define I8254x_REG_TDLEN     0x03808
#define I8254x_REG_TDH       0x03810
#define I8254x_REG_TDT       0x03818

#define I8254x_EERD_START (1 << 0)
#define I8254x_EERD_DONE (1 << 4)


#define I8254x_EECD_SK (1 << 0)
#define I8254x_EECD_CS (1 << 1)
#define I8254x_EECD_DI (1 << 2)
#define I8254x_EECD_DO (1 << 3)

#define I8254x_EECD_EE_PRES (1 << 8)
#define I8254x_EECD_EE_REQ  (1 << 6)
#define I8254x_EECD_EE_GNT (1 << 7)

#define I8254x_CTRL_RESET (1 << 26)
#define I8254x_CTRL_ASDE  (1 << 5)
#define I8254x_CTRL_SLU   (1 << 6)
#define I8254x_CTRL_VME   (1 << 30)

/* Receive Address Registers (8-byte stride) */
#define I8254x_REG_RAL(n)    (0x05400 + ((n) * 8))
#define I8254x_REG_RAH(n)    (0x05404 + ((n) * 8))

void init_82540EM_driver(pci_device_t* dev);
#endif