#ifndef INCLUDE_ARP_H
#define INCLUDE_ARP_H

#include "networking.h"
#include <stdint.h>

struct net_interface;
typedef struct net_interface net_interface_t;

#define ARP_HTYPE_ETH 0x1
#define ARP_PTYPE_IPv4 0x0800
#define ARP_OP_REQ_MAC 0x1
#define ARP_OP_REPLY_MAC 0x2

#define ARP_MAC_HLEN 6
#define ARP_IP_PLEN 4

#define ARP_HDR_RET_SUCCESS 0x0
#define ARP_HDR_RET_DATA_OVERFLOW 0x1
#define ARP_HDR_RET_NO_ROUTE 0x2

#define ARP_CACHE_TIMEOUT 30

typedef struct {
    uint16_t htype;
    uint16_t ptype;
    uint8_t hlen; // 6
    uint8_t plen; // 4
    uint16_t opcode;
    uint8_t src_mac[6];
    uint32_t src_ip;
    uint8_t dst_mac[6];
    uint32_t dst_ip;
}__attribute__((packed)) arp_header_t;

typedef struct arp_mac_cache {
    uint32_t ip_addr;
    uint16_t timeout;
    
    uint8_t mac[6];
    struct arp_mac_cache* next;
} arp_mac_cache_t;

/**
 * arp_add_header:
 * Adds an ARP header to a ethernet packet by prepending to previous headers
 * @param iface The network interface
 * @param data The packet buffer pointer
 * @param write_off A pointer to a write offset into the buffer (should be just above where the header will be added)
 * @param opcode The ARP opcode to specify (request or reply)
 * @param dst_mac A uint8_t[6] array of the destination mac address or nullptr if unknown (for ARP requests)
 * @param dst_ip The destination IP address to specify
 * 
 * @return ARP_HDR_RET_SUCCESS on success, an error otherwise
 */
uint8_t arp_add_header(net_interface_t* iface,uint8_t* data, uint32_t* write_off,uint16_t opcode, uint8_t* dst_mac,uint32_t dst_ip);

/**
 * arp_send_request:
 * Sends an ARP request for the specified IP address
 *
 * @param dst_ip The IP address to request the MAC address for
 */
void arp_send_request(uint32_t dst_ip);

/**
 * arp_handle_packet:
 * Extracts the ARP header of an incoming network packet, may cache the MAC address of the sender, and may send an ARP response if the packet is an ARP request for this machine's IP address
 * @param data The raw packet data
 * @param write_off The offset into the packet where the ARP header starts
 * @param total_len The total length of the packet
 */
void arp_handle_packet(uint8_t* data, uint32_t write_off, uint32_t total_len);

/**
 * arp_lookup:
 * Looks up the MAC address for a given IP address in the ARP cache. If not found, sends an ARP request and waits for a response.
 * @param ip_addr The IP address to look up
 * @param mac_out A pointer to a uint8_t[6] array where the MAC address will be stored if found
 */
void arp_lookup(uint32_t ip_addr,uint8_t* mac_out);

void arp_timer_callback();
#endif