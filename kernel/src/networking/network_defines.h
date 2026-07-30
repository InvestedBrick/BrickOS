#ifndef INCLUDE_NETWORK_DEFINES_H
#define INCLUDE_NETWORK_DEFINES_H

#include <stdint.h>

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

#define INADDR_LOOPBACK 0x7f000001
#define INADDR_ANY      0x00000000

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

#endif