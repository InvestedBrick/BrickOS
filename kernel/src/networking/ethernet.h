#ifndef INCLUDE_ETHERNET_H
#define INCLUDE_ETHERNET_H
#include <stdint.h>
#include "networking.h"

#define ETHERTYPE_ARP  0x0806
#define ETHERTYPE_IPv4 0x0800
#define ETHERTYPE_IPv6 0x86dd

#define ETH_HDR_RET_SUCCESS 0x0
#define ETH_HDR_RET_DATA_OVERFLOW 0x1
typedef struct {
    uint8_t dst_mac[6];
    uint8_t src_mac[6];
    uint16_t type;
} __attribute__((packed)) ethernet_header_t;


/**
 * ethernet_handle_packet:
 * Extracts the ethernet header of an incoming network packet and passes the packet down the rest of the network stack
 * @param data The raw packet data
 * @param len The length of the packet
 */
void ethernet_handle_packet(uint8_t* data, uint32_t len);

/**
 * ethernet_add_header:
 * Adds an ethernet header to a ethernet packet by prepending to previous headers
 * @param iface The network interface
 * @param data The packet buffer pointer
 * @param write_off A pointer to a write offset into the buffer (should be just above where the header will be added)
 * @param dst_mac A uint8_t[6] array of the destination mac address
 * @param ethertype The ethertype to specify
 * @return ETH_HDR_RET_SUCCESS on success, an error otherwise
 */
uint8_t ethernet_add_header(net_interface_t* iface,uint8_t* data, uint32_t* write_off,uint8_t* dst_mac,uint16_t ethertype);
#endif