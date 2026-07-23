#ifndef INCLUDE_ICMP_H
#define INCLUDE_ICMP_H

#include <stdint.h>
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

typedef struct {
    uint8_t icmp_type;
    uint8_t icmp_code;
    uint16_t checksum;
    uint8_t may_be_used[4];
}icmp_header_t;

typedef struct {
    uint16_t echo_ident;
    uint16_t echo_seq;
}icmp_echo_t;

typedef struct {
    uint16_t info_ident;
    uint16_t info_seq;
}icmp_info_t;

typedef struct {
    uint16_t time_stmp_ident;
    uint16_t time_stmp_seq;
    uint32_t originate_timestmp;
    uint32_t recv_timestmp;
    uint32_t transmit_timestmp;
}icmp_timestamp_t;

typedef struct {
    uint8_t ptr;
}icmp_param_problem_t;

typedef struct {
    uint32_t gateway_ip_addr;
}icmp_redir_t;


typedef struct{
    uint8_t icmp_type;
    uint8_t icmp_code;

    uint32_t may_be_used;

    uint8_t* extra_payload;
    uint32_t extra_payload_len;
}icmp_send_data_t;


/**
 * icmp_handle_packet:
 * Reads the ICMP header of a packet and takes action depending on the type
 * @param data The data starting at the icmp header
 * @param len The length of the header + any extra payloads
 * @param src_ip The IPv4 address of the message source
 */
void icmp_handle_packet(uint8_t* data, uint32_t len, uint32_t src_ip);

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


void icmp_handle_echo_reply(icmp_header_t* icmp_hdr, uint32_t total_len);
void icmp_handle_dest_unreachable(icmp_header_t* icmp_hdr, uint32_t total_len);
void icmp_handle_source_quench(icmp_header_t* icmp_hdr, uint32_t total_len);
void icmp_handle_redirect(icmp_header_t* icmp_hdr, uint32_t total_len);
void icmp_handle_echo_request(icmp_header_t* icmp_hdr, uint32_t total_len, uint32_t src_ip);
void icmp_handle_time_exceeded(icmp_header_t* icmp_hdr, uint32_t total_len);
void icmp_handle_parameter_problem(icmp_header_t* icmp_hdr, uint32_t total_len);
void icmp_handle_timestamp(icmp_header_t* icmp_hdr, uint32_t total_len);
void icmp_handle_timestamp_reply(icmp_header_t* icmp_hdr, uint32_t total_len);
void icmp_handle_info_request(icmp_header_t* icmp_hdr, uint32_t total_len);
void icmp_handle_info_reply(icmp_header_t* icmp_hdr, uint32_t total_len);

#endif