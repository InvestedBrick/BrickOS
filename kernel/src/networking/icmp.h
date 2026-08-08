#ifndef INCLUDE_ICMP_H
#define INCLUDE_ICMP_H

#include <stdint.h>
#include "userspace_api/socket.h"
#include "ip.h"
#define ICMP_TYPE_ECHO_REPLY 0
#define ICMP_TYPE_DEST_UNR_MSG 3
#define ICMP_TYPE_SRC_QUENCH_MSG 4
#define ICMP_TYPE_REDIR_MSG 5
#define ICMP_TYPE_ECHO_MSG 8
#define ICMP_TYPE_TIME_EXC_MSG 11
#define ICMP_TYPE_PARAM_PRBLM_MSG 12
#define ICMP_TYPE_TIMESTMP_MSG 13
#define ICMP_TYPE_TIMESTMP_REPLY 14
#define ICMP_TYPE_INFO_REQ_MSG 15
#define ICMP_TYPE_INFO_REPLY_MSG 16

#define ICMP_HDR_RET_SUCCESS 0x0
#define ICMP_HDR_RET_DATA_OVERFLOW 0x1
#define ICMP_HDR_RET_INVALID_TYPE 0x2

#define ICMP_REDIR_NETWORK 0
#define ICMP_REDIR_TOS_NETWORK 2

#define ICMP_REDIR_HOST 1
#define ICMP_REDIR_TOS_HOST 3

#define DEFAULT_ICMP_HDR_SIZE 8

typedef struct {
    uint16_t echo_ident;
    uint16_t echo_seq;
}__attribute__((packed)) icmp_echo_t;

typedef struct {
    uint16_t info_ident;
    uint16_t info_seq;
}__attribute__((packed)) icmp_info_t;

typedef struct {
    uint16_t time_stmp_ident;
    uint16_t time_stmp_seq;
    uint32_t originate_timestmp;
    uint32_t recv_timestmp;
    uint32_t transmit_timestmp;
}__attribute__((packed)) icmp_timestamp_t;

typedef struct {
    uint8_t ptr;
}__attribute__((packed)) icmp_param_problem_t;

typedef struct {
    uint32_t gateway_ip_addr;
}__attribute__((packed)) icmp_redir_t;

typedef struct {
    uint8_t icmp_type;
    uint8_t icmp_code;
    uint16_t checksum;
    union {
        uint32_t unused;
        icmp_echo_t echo;
        icmp_redir_t redir;
        icmp_param_problem_t param_problem;
        icmp_timestamp_t timestamp;
        icmp_info_t info;
    }un;

}__attribute__((packed)) icmp_header_t;

typedef struct{
    uint8_t icmp_type;
    uint8_t icmp_code;

    uint32_t may_be_used;

    uint8_t* extra_payload;
    uint32_t extra_payload_len;
}icmp_send_data_t;

typedef struct {
    generic_proto_socket_t sock;
}icmp_socket_t;

extern icmp_socket_t* icmp_sock_head;
extern mutex_t icmp_sock_queue_lock;

#define ICMP_RET_FAIL -1
#define ICMP_RET_SUCCESS 0

/**
 * get_true_icmp_header_size:
 * Returns the actually needed icmp header size based on the type
 * @param icmp_type The ICMP type
 * @return the headersize
 */
uint16_t get_true_icmp_header_size(uint8_t icmp_type);

/**
 * icmp_handle_packet:
 * Reads the ICMP header of a packet and takes action depending on the type
 * @param data The data starting at the ipv4 header
 * @param len The length of the ip and icmp header + any extra payloads
 */
void icmp_handle_packet(uint8_t* data, uint32_t len);

/**
 * icmp_add_hdr:
 * Adds an ICMP header and additional payloads to a packet
 * @param data The packet buffer
 * @param write_off A pointer to the write offset
 * @param icmp_type The ICMP type
 * @param icmp_code The ICMP code
 * @param may_be_used_dword The 32 bit dword that may contain additonal data (assumed to be (upper_16 << 16 | lower_16 ))
 * @param extra_payload The optional extra payload after the actual header for e. g. echo messages
 * @param extra_payload_len The length of the extra payload 
 */
uint8_t icmp_add_hdr(uint8_t* data, uint32_t* write_off, uint8_t icmp_type, uint8_t icmp_code, uint32_t may_be_used_dword, uint8_t* extra_payload, uint32_t extra_payload_len);


void icmp_handle_dest_unreachable(ipv4_header_t* ip_hdr, uint8_t ip_hlen,uint32_t total_len);
void icmp_handle_source_quench(ipv4_header_t* ip_hdr, uint8_t ip_hlen,uint32_t total_len);
void icmp_handle_redirect(ipv4_header_t* ip_hdr, uint8_t ip_hlen,uint32_t total_len);
void icmp_handle_echo_request(ipv4_header_t* ip_hdr, uint8_t ip_hlen,uint32_t total_len);
void icmp_handle_time_exceeded(ipv4_header_t* ip_hdr, uint8_t ip_hlen,uint32_t total_len);
void icmp_handle_parameter_problem(ipv4_header_t* ip_hdr, uint8_t ip_hlen,uint32_t total_len);
void icmp_handle_timestamp(ipv4_header_t* ip_hdr, uint8_t ip_hlen,uint32_t total_len);
void icmp_handle_info_request(ipv4_header_t* ip_hdr, uint8_t ip_hlen,uint32_t total_len);
void icmp_handle_info_reply(ipv4_header_t* ip_hdr, uint8_t ip_hlen,uint32_t total_len);

/**
 * icmp_handle_socket_packet:
 * Handles all icmp packets that need to be delivered to sockets
 * @param ip_hdr The IP header of the passed-along data
 * @param ip_hlen The IP header length
 * @param total_len The total length of the data
 */
void icmp_handle_socket_packet(ipv4_header_t* ip_hdr, uint8_t ip_hlen, uint32_t total_len);

#endif