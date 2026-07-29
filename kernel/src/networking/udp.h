#ifndef INCLUDE_UDP_H
#define INCLUDE_UDP_H

#include <stdint.h>

#include "networking.h"
#include "../processes/user_process.h"
#include "../processes/spinlocks.h"
#include "../processes/scheduler.h"

struct net_interface;
typedef struct net_interface net_interface_t;

typedef struct udp_header{
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length; // len of hdr + following data
    uint16_t checksum;
}__attribute__((packed)) udp_header_t;

// used for checksum
typedef struct pseudo_ip_hdr{
    uint32_t src_addr;
    uint32_t dst_addr;
    uint8_t zero;
    uint8_t protocol;
    uint16_t udp_length;
}__attribute__((packed)) pseudo_ip_hdr_t; 

typedef struct udp_waiting_thread{
    thread_t* thread;
    struct udp_waiting_thread* next;
}udp_waiting_thread_t;

typedef struct udp_recvd_packet {
    uint8_t* data;
    uint32_t data_len;

    struct udp_recvd_packet* next;
}udp_recvd_packet_t;

typedef struct udp_socket{
    uint32_t ip_addr;
    uint16_t port;

    user_process_t* owner_proc;
    mutex_t lock;

    udp_waiting_thread_t* wait_queue;
    udp_recvd_packet_t* rx_queue;

    struct udp_socket* next; // everything is a linked list
}udp_socket_t;

#define UDP_HDR_RET_SUCESS 0x0
#define UDP_HDR_RET_DATA_OVERFLOW 0x1

/**
 * init_udp_sock_queue:
 * Initializes the udp socket queue
 */
void init_udp_sock_queue();

/**
 * udp_add_header:
 * Adds a UDP header to an IP packet
 * @param iface The network interface on which this packet is being constructed
 * @param data The buffer of the packet
 * @param write_off A pointer to a write offset into the buffer (should be just above where the header will be added)
 * @param src_port The source port of the packet
 * @param dst_port The destination port of the packet
 * @param post_hdr_data_len The length of the data after the UDP header (needed for checksum)
 * @param post_hdr_data The data after the UDP header (needed for checksum)
 * @param dst_addr The destination IP address (needed for checksum)
 * 
 * @return UDP_HDR_RET_SUCESS on success, an error otherwise
 */
uint8_t udp_add_header(net_interface_t* iface, uint8_t* data, uint32_t* write_off, uint16_t src_port, uint16_t dst_port,uint32_t post_hdr_data_len,uint8_t* post_hdr_data, uint32_t dst_addr);

/**
 * init_udp_socket:
 * creates and initialises an UDP socket
 * @param ip_addr The IPv4 address that should be associated with this socket (may also be INADDR_ANY)
 * @param port The port associated with this socket
 * @return The udp socket to assign as the "prot_sock" in a socket_t struct
 */
udp_socket_t* init_udp_socket(uint32_t ip_addr, uint16_t port);
#endif