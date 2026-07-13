#ifndef INCLUDE_NETWORKING_H
#define INCLUDE_NETWORKING_H

#define IP_ADDR_UNASSIGNED 0x0
#define IP_TESTING 0xc0a86402
#define NETMASK_DEFAULT 0xffffff00

#include "../drivers/PCI/pci.h"
#include "arp.h"
#include <stdint.h>

typedef struct {
    pci_device_t* dev;
    uint8_t mac_addr[6];

    uint32_t ip_addr;
    uint32_t netmask;
    uint32_t gateway;

    arp_mac_cache_t* arp_cache_head; 

    uint32_t (*send)(void*,uint32_t);
}generic_nic_driver_t;

extern generic_nic_driver_t* nic_driver;

typedef struct{
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
}__attribute__((packed)) udp_header_t;

typedef struct {
    uint8_t op_code;
    uint8_t htype;
    uint8_t hlen;
    uint8_t hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    uint32_t client_ip_addr;
    uint32_t your_ip_addr;
    uint32_t server_ip_addr;
    uint32_t gateway_ip_addr;
    uint8_t client_hardware_addr[16];
    uint8_t server_name[64];
    uint8_t boot_file_name[128];
    uint8_t options[0];
}__attribute__((packed)) dhcp_header_t;

#define NETW_DEV_ID_82540EM 0x100e
void setup_network_driver();

uint16_t switch_endian16(uint16_t nb);
uint32_t switch_endian32(uint32_t nb);
#endif