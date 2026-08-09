#ifndef INCLUDE_NETWORK_DEFINES_H
#define INCLUDE_NETWORK_DEFINES_H

#include <stdint.h>
/**
 * IMPORTANT: all ports and ip addresses must be passed in normal host byte order
 * The OS handles the endianness switching
 */
typedef enum {
    INET_FAM_IPv4,
    INET_FAM_IPv6,
}inet_family_e;

typedef enum {
    SOCKET_TYPE_DGRAM,
    SOCKET_TYPE_STREAM,
    SOCKET_TYPE_RAW,
    SOCKET_TYPE_UNUSED
}socket_type_e;

typedef enum {
    SOCKET_INET,
    SOCKET_UNUSED,
}socket_domain_e;

#define IP_PROTOCOL_ICMP 1
#define IP_PROTOCOL_TCP  6
#define IP_PROTOCOL_UDP  17
#define IP_PROTOCOL_RAW1  253
#define IP_PROTOCOL_RAW2  254

#define INADDR_LOOPBACK 0x7f000001
#define INADDR_ANY      0x00000000

// socket opt level (need to be above protocol numbers, as they might also be valid levels)
#define SOL_SOCKET 256
#define SOL_IP 257

#define IP_TOS        (SOL_IP << 8 | 0x1)
#define IP_TTL        (SOL_IP << 8 | 0x2)
#define IP_HDRINCL    (SOL_IP << 8 | 0x3)

// TODO: implement sendbuffer in TCP implementation
#define SO_RCVTIMEOUT (SOL_SOCKET << 8 | 0x1)
#define SO_SNDTIMEOUT (SOL_SOCKET << 8 | 0x2)
#define SO_REUSEADDR  (SOL_SOCKET << 8 | 0x3)
#define SO_RCVBUF     (SOL_SOCKET << 8 | 0x4)
#define SO_SNDBUF     (SOL_SOCKET << 8 | 0x5)

typedef struct {
    uint16_t inet_family;
    uint16_t inet_port;
    uint32_t inet_addr;
    uint8_t pad[8]; // to make it castable to sockaddr_t
}__attribute__((packed)) in_sockaddr_t;

typedef struct {
    uint16_t inet_family;
    uint8_t sa_data[14];
}__attribute__((packed)) sockaddr_t;


#define MSG_DONTWAIT (1 << 0)
#define MSG_PEEK (1 << 1)

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

#define IP_VERSION_4 0x4

#define IP_TOS_DELAY_LOW        (1 << 3)
#define IP_TOS_THROUGHPUT_HIGH  (1 << 4)
#define IP_TOS_RELIABILITY_HIGH (1 << 5)

#define IP_TOS_PREC_ROUTINE         0b000
#define IP_TOS_PREC_PRIORITY        0b001
#define IP_TOS_PREC_IMMEDIATE       0b010
#define IP_TOS_PREC_FLASH           0b011
#define IP_TOS_PREC_FLASH_OVERRIDE  0b100
#define IP_TOS_PREC_CRITIC_ECP      0b101
#define IP_TOS_PREC_INTERNETWORK    0b110
#define IP_TOS_PREC_NETWORK_CONTROL 0b111

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
#endif