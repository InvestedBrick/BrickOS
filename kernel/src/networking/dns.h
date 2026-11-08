#ifndef INCLUDE_DNS_H
#define INCLUDE_DNS_H
#include <stdint.h>

/*
 * DNS flags field layout (host byte order, bit 15 = MSB):
 *
 *   15  14-11   10  9   8   7   6  5   4   3-0
 *   QR  Opcode  AA  TC  RD  RA  Z  AD  CD  RCODE
 */

/* QR: query/response (bit 15) */
#define DNS_FLAG_QR_QUERY       0x0000u
#define DNS_FLAG_QR_RESPONSE    0x8000u

#define DNS_UDP_PORT 53
#define DNS_MAX_LABEL_LEN 63
#define DNS_MAX_DOMAIN_NAME_LEN 255

/* Opcode (bits 14-11) */
#define DNS_OPCODE_SHIFT        11
#define DNS_OPCODE_MASK         0x7800u
#define DNS_FLAG_OPCODE_QUERY   (0u << DNS_OPCODE_SHIFT)  /* standard query */
#define DNS_FLAG_OPCODE_IQUERY  (1u << DNS_OPCODE_SHIFT)  /* inverse query (obsolete) */
#define DNS_FLAG_OPCODE_STATUS  (2u << DNS_OPCODE_SHIFT)  /* server status request */
#define DNS_FLAG_OPCODE_NOTIFY  (4u << DNS_OPCODE_SHIFT)  /* RFC 1996 */
#define DNS_FLAG_OPCODE_UPDATE  (5u << DNS_OPCODE_SHIFT)  /* RFC 2136 */

/* Single-bit flags */
#define DNS_FLAG_AA             0x0400u  /* bit 10: authoritative answer */
#define DNS_FLAG_TC             0x0200u  /* bit 9:  truncated */
#define DNS_FLAG_RD             0x0100u  /* bit 8:  recursion desired */
#define DNS_FLAG_RA             0x0080u  /* bit 7:  recursion available */
#define DNS_FLAG_Z              0x0040u  /* bit 6:  reserved, must be zero */
#define DNS_FLAG_AD             0x0020u  /* bit 5:  authentic data (DNSSEC) */
#define DNS_FLAG_CD             0x0010u  /* bit 4:  checking disabled (DNSSEC) */

/* RCODE (bits 3-0) */
#define DNS_RCODE_SHIFT         0
#define DNS_RCODE_MASK          0x000fu
#define DNS_FLAG_RCODE_NOERROR  0x0000u
#define DNS_FLAG_RCODE_FORMERR  0x0001u
#define DNS_FLAG_RCODE_SERVFAIL 0x0002u
#define DNS_FLAG_RCODE_NXDOMAIN 0x0003u
#define DNS_FLAG_RCODE_NOTIMP   0x0004u
#define DNS_FLAG_RCODE_REFUSED  0x0005u

#define DNS_FLAGS_IS_RESPONSE(f)  (((f) & DNS_FLAG_QR_RESPONSE) != 0)
#define DNS_FLAGS_OPCODE(f)       (((f) & DNS_OPCODE_MASK) >> DNS_OPCODE_SHIFT)
#define DNS_FLAGS_RCODE(f)        ((f) & DNS_RCODE_MASK)

typedef enum {
    DNS_TYPE_A     = 1,
    DNS_TYPE_NS    = 2,
    DNS_TYPE_CNAME = 5,
    DNS_TYPE_SOA   = 6,
    DNS_TYPE_PTR   = 12,
    DNS_TYPE_OPT   = 41
} dns_type_t;

typedef enum {
    DNS_CLASS_IN = 1
}dns_class_e;

/**
 * buildup of dns question:
 * QNAME - variable name of domain, encoding example (www.example.com): '[len of 'www'] 'www' [len of 'example] 'example' [len of 'com] 'com' 0x00'
 * QTYPE  - 16 bits for type of dns record requested
 * QCLASS - 16 bits class of DNS record (IN for internet)
 */ 


typedef struct {
    uint16_t xid;
    uint16_t flags;
    uint16_t n_questions;
    uint16_t n_answers;
    uint16_t n_auth_rrs;
    uint16_t n_add_rrs;
} __attribute__((packed)) dns_header_t;

typedef struct {
    uint16_t qtype;
    uint16_t qclass;
} __attribute__((packed)) dns_question_tail_t;

/**
 * buildup of dns answer:
 * ANAME - variable name of domain
 * ATYPE  - 16 bits for type of dns record requested
 * ACLASS - 16 bits class of DNS record (IN for internet)
 * TTL - how long the answer is valid in seconds
 * address - the returned address
 */ 

typedef struct {
    uint16_t atype;
    uint16_t aclass;
    uint32_t ttl;
    uint16_t rdlen;
    uint32_t rdata;
} __attribute__((packed)) dns_rr_tail;

typedef enum {
    DNS_RET_SUCCESS,
    DNS_RET_FAILURE
}dns_ret_code_e;

/**
 * dns_init:
 * Creates and binds the kernel Domain Name System socket
 */
void dns_init();
#endif /* INCLUDE_DNS_H */