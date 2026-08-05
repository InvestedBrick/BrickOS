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


#define MSG_DONTWAIT (0 << 1)
#define MSG_PEEK (1 << 1)
#endif