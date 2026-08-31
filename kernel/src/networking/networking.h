#ifndef INCLUDE_NETWORKING_H
#define INCLUDE_NETWORKING_H

#define IP_ADDR_UNASSIGNED 0x0
#define IP_TESTING 0xc0a86402
#define ROUTER_IP 0xc0a80001
#define NETMASK_DEFAULT 0xffffff00

#define DEFAULT_MTU 1500
#define MAX_ROUTING_TABLE_ENTRIES 10

#include "../drivers/PCI/pci.h"
#include "../processes/spinlocks.h"
#include "arp.h"
#include "udp.h"
#include "dhcp.h"
#include <stdint.h>

struct arp_mac_cache;
typedef struct arp_mac_cache arp_mac_cache_t;

struct udp_header;
typedef struct udp_header udp_header_t;

struct pseudo_ip_hdr;
typedef struct pseudo_ip_hdr pseudo_ip_hdr_t;

#define LOOPBACK_ADDR 0x7f000001

typedef struct net_interface {
    char name[16];
    uint8_t mac_addr[6];
    
    dhcp_client_t dhcp;
    
    uint32_t mtu;
    
    struct arp_mac_cache* arp_cache_head;
    spinlock_t mac_cache_lock; 
    uint32_t (*send)(void*,uint32_t);

}net_interface_t;

typedef struct {
    uint32_t network;
    uint32_t netmask;
    uint32_t gateway;
    net_interface_t* iface;
    
}route_t;

typedef struct {
    route_t routes[MAX_ROUTING_TABLE_ENTRIES];
    uint32_t n_routes;
}routing_table_t;

extern routing_table_t routing_table;

#define NETW_DEV_ID_82540EM 0x100e
void setup_network_driver();

uint16_t switch_endian16(uint16_t nb);
uint32_t switch_endian32(uint32_t nb);

/**
 * compute_checksum:
 * Computes the 16 bit checksum over some data / header. Returns the checksum in host byte order.
 * @param hdr pointer to the data / header to compute the checksum over
 * @param len length of the data / header in bytes
 * Returns: the computed checksum in host byte order
 */
uint16_t compute_checksum(uint8_t* hdr,uint32_t len);

/**
 * compute_udp_checksum:
 * computes the 16 bit checksum for udp header, the associated pseudo ip header and the following data
 * @param udp_hdr A pointer to the UDP header with checksum = 0
 * @param pseudo_ip_header The filled out (network byte order) pseudo ip header needed for udp checksum
 * @param data The data following the UDP header
 * @param data_len The length of the data following the UDP header
 * @return the computed checksum in host byte order
 */
uint16_t compute_udp_checksum(udp_header_t* udp_hdr, pseudo_ip_hdr_t* pseudo_ip_hdr, uint8_t* data, uint32_t data_len);

/**
 * update_or_insert_route:
 * Tries to update an entry in the routing table and inserts new one if not found
 * @param iface The interface to be assigned to the route
 * @param network The network of the route
 * @param netmask The netmask for the route
 * @param gateway THe gateway to be assigned to the route
 */
void update_or_insert_route(net_interface_t* iface, uint32_t network, uint32_t netmask, uint32_t gateway);
#endif