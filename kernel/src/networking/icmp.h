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
    uint32_t originate_timstmp;
    uint32_t recv_timstmp;
    uint32_t transmit_timstmp;
}icmp_timestamp_t;

typedef struct {
    uint8_t ptr;
}icmp_param_problem_t;

typedef struct {
    uint32_t gateway_ip_addr;
}icmp_redir_t;

void icmp_handle_packet(uint8_t* data, uint32_t len);

void icmp_handle_echo_reply(icmp_header_t* icmp_hdr, uint32_t total_len);
void icmp_handle_dest_unreachable(icmp_header_t* icmp_hdr, uint32_t total_len);
void icmp_handle_source_quench(icmp_header_t* icmp_hdr, uint32_t total_len);
void icmp_handle_redirect(icmp_header_t* icmp_hdr, uint32_t total_len);
void icmp_handle_echo_request(icmp_header_t* icmp_hdr, uint32_t total_len);
void icmp_handle_time_exceeded(icmp_header_t* icmp_hdr, uint32_t total_len);
void icmp_handle_parameter_problem(icmp_header_t* icmp_hdr, uint32_t total_len);
void icmp_handle_timestamp(icmp_header_t* icmp_hdr, uint32_t total_len);
void icmp_handle_timestamp_reply(icmp_header_t* icmp_hdr, uint32_t total_len);
void icmp_handle_info_request(icmp_header_t* icmp_hdr, uint32_t total_len);
void icmp_handle_info_reply(icmp_header_t* icmp_hdr, uint32_t total_len);

#endif