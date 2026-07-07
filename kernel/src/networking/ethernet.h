#ifndef INCLUDE_ETHERNET_H
#define INCLUDE_ETHERNET_H
#include <stdint.h>
#define ETHERTYPE_ARP  0x0806
#define ETHERTYPE_IPv4 0x0800
#define ETHERTYPE_IPv6 0x86dd

typedef struct {
    uint8_t dst_mac[6];
    uint8_t src_mac[6];
    uint16_t type;
} __attribute__((packed)) ethernet_header_t;

void ethernet_handle_packet(uint8_t* data, uint32_t len);

#endif