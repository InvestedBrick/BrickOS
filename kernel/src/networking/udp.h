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

void init_sock_queue();

uint8_t udp_add_header(net_interface_t* iface, uint8_t* data, uint32_t* write_off, uint16_t src_port, uint16_t dst_port,uint32_t post_hdr_data_len,uint8_t* post_hdr_data, uint32_t dst_addr);
#endif