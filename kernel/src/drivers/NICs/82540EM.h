#ifndef INCLUDE_82540EM_H
#define INCLUDE_82540EM_H
#include "../PCI/pci.h"
#include "../../networking/networking.h"

typedef struct {
    uint64_t buff_addr; // phys addr
    uint16_t len;
    uint8_t cso;
    uint8_t cmd;
    uint8_t status;
    uint8_t css;
    uint16_t special;
}__attribute__((packed)) i8254x_tx_descriptor_t;

typedef struct {
    uint64_t buff_addr; // phys addr
    uint16_t len;
    uint16_t pcs; // packet checksum, only used on 82544gc/el
    uint8_t status;
    uint8_t errors;
    uint16_t special; // only used on 82544gc/el
}__attribute__((packed)) i8254x_rx_descriptor_t;


// needs to be multiple of 8, each descriptor gets a buffer of one page
#define I8254x_N_TX_DESCRS 16

#define I8254x_N_RX_DESCRS 32

typedef struct{
    uint64_t reg_base_addr;
    uint64_t flash_base_addr;
    uint64_t io_reg_base_addr;

    i8254x_tx_descriptor_t* tx_ring;
    i8254x_rx_descriptor_t* rx_ring;

    // RX vars
    uint32_t rx_next; 
    uint8_t accumulating;
    uint64_t total_size;
    uint32_t start_idx;
    

    // TX vars
    uint32_t tx_cleanup; // index responsible to clean up physically allocated memory buffers for TX

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
#define I8254x_EECD_EE_GNT  (1 << 7)

#define I8254x_CTRL_RESET (1 << 26)
#define I8254x_CTRL_ASDE  (1 << 5)
#define I8254x_CTRL_SLU   (1 << 6)
#define I8254x_CTRL_VME   (1 << 30)

#define I8254x_TCTL_EN  (1 << 1)
#define I8254x_TCTL_PSP (1 << 3)

#define I8254x_RCTL_EN         (1 << 1)
#define I8254x_RCTL_UPE        (1 << 3)
#define I8254x_RCTL_MPE        (1 << 4)
#define I8254x_RCTL_LPE        (1 << 5)
#define I8254x_RCTL_BAM        (1 << 15)
#define I8254x_RCTL_BSEX       (1 << 25)
#define I8254x_RCTL_BSIZE_4096 (0b11 << 16)

#define I8254x_IMS_LSC  (1 << 2)
#define I8254x_IMS_RXO  (1 << 6)
#define I8254x_IMS_RXT0 (1 << 7)

#define I8254x_STATUS_LU  (1 << 1)

#define I8254x_TX_CMD_EOP  (1 << 0)
#define I8254x_TX_CMD_IFCS (1 << 1)
#define I8254x_TX_CMD_RS   (1 << 3)

#define I8254x_TX_STAT_DD   (1 << 0)
#define I8254x_RX_STAT_DD   (1 << 0)
#define I8254x_RX_STAT_EOP  (1 << 1)

/* Receive Address Registers (8-byte stride) */
#define I8254x_REG_RAL(n)    (0x05400 + ((n) * 8))
#define I8254x_REG_RAH(n)    (0x05404 + ((n) * 8))

generic_nic_driver_t* init_82540EM_driver(pci_device_t* dev);

uint32_t i8254x_send(void* data, uint32_t len);
#endif