#ifndef INCLUDE_UDP_H
#define INCLUDE_UDP_H

#include <stdint.h>
typedef struct{
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length; // len of hdr + following data
    uint16_t checksum;
}__attribute__((packed)) udp_header_t;

// used for checksum
typedef struct {
    uint32_t src_addr;
    uint32_t dst_addr;
    uint8_t zero;
    uint8_t protocol;
    uint16_t udp_length;
}__attribute__((packed)) pseudo_ip_hdr_t; 

#define UDP_HDR_RET_SUCESS 0x0
#define UDP_HDR_RET_DATA_OVERFLOW 0x1

uint8_t udp_add_header(net_interface_t* iface, uint8_t* data, uint32_t* write_off, uint16_t src_port, uint16_t dst_port,uint32_t post_hdr_data_len,uint8_t* post_hdr_data, uint32_t dst_addr);
#endif