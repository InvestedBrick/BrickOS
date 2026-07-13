#ifndef INCLUDE_IP_H
#define INCLUDE_IP_H

#include <stdint.h>
#define IP_VERSION_4 0x4


/**
 * type of service:
 * 0            2   3          4           5       6    7 
 * [ precedence | delay | throughput | relibility| 0 | 0 ]
 */

#define IP_TOS_DELAY_LOW       (1 << 3)
#define IP_TOS_THROUGHPUT_HIGH (1 << 4)
#define IP_TOS_RELIBILITY_HIGH (1 << 5)

#define IP_TOS_PREC_ROUTINE         0b000
#define IP_TOS_PREC_PRIORITY        0b001
#define IP_TOS_PREC_IMMEDIATE       0b010
#define IP_TOS_PREC_FLASH           0b011
#define IP_TOS_PREC_FLASH_OVERRIDE  0b100
#define IP_TOS_PREC_CRITIC_ECP      0b101
#define IP_TOS_PREC_INTERNETWORK    0b110
#define IP_TOS_PREC_NETWORK_CONTROL 0b111


#define IP_FLAGS_DONT_FRAGMENT  (1 << 1)
#define IP_FLAGS_MORE_FRAGMENTS (1 << 2)

// fragment offset is measured in units of 8 bytes to where the fragment belongs

#define IP_PROTOCOL_ICMP 0x1
#define IP_PROTOCOL_TCP 0x6
#define IP_PROTOCOL_UDP 0x11

#define IP_OPTION_TYPE_END 0
typedef struct {
    uint8_t version_ihl; // 4 bits version, 4 bits internet header length (in 32 bit words)
    uint8_t type_of_service;
    uint16_t total_length; // len of data in 8-bit bytes
    uint16_t ident; // chosen by sender
    uint16_t flags_fragment_offset; // 3 bits flags, 13 bits fragment offset
    uint8_t time_to_live;
    uint8_t protocol;
    uint16_t header_checksum;
    uint32_t src_ip;
    uint32_t dst_ip;
}__attribute__((packed)) ipv4_header_t;

#define IP_HDR_RET_SUCCESS 0x0
#define IP_HDR_RET_DATA_OVERFLOW 0x1

/**
 * ip_add_header:
 * Adds an IP header to a ethernet packet by prepending to previous headers
 * @param data The packet buffer pointer
 * @param write_off A pointer to a write offset into the buffer (should be just above where the header will be added)
 * @param dst_addr The destination IP address to specify
 * @param prot The IP protocol to specify (ICMP, TCP, UDP)
 * @param tos The type of service to specify (see IP_TOS_* macros)
 * @param ttl The time to live to specify
 * @param id The identification number to specify
 * @param df Whether to set the "Don't Fragment" flag (1) or not (0)
 * 
 * @return IP_HDR_RET_SUCCESS on success, an error otherwise
 */
uint8_t ip_add_header(uint8_t* data, uint32_t* write_off,uint32_t dst_addr,uint8_t prot, uint8_t tos, uint8_t ttl,uint16_t id,uint8_t df);

/**
 * ip_handle_packet:
 * Extracts the IP header of an incoming network packet, and may pass the payload to the appropriate protocol handler (ICMP, TCP, UDP)
 * @param data The raw packet data
 * @param write_off The offset into the packet where the IP header starts
 * @param total_len The total length of the packet
 */
void ip_handle_packet(uint8_t* data, uint32_t write_off, uint32_t total_len);
#endif