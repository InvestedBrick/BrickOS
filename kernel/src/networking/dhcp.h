#ifndef INCLUDE_DHCP_H
#define INCLUDE_DHCP_H

#include <stdint.h>
#include <shared/network_defines.h>
#include "../utilities/queue.h"
#include "networking.h"

#define DHCP_CLIENT_PORT 68
#define DHCP_SERVER_PORT 67

#define DHCP_HTYPE_ETH 1
#define DHCP_HLEN_MAC 6

#define DHCP_OP_BOOTREQUEST 1
#define DHCP_OP_BOOTREPLY 2

// replies will be sent via broadcast 
#define DHCP_RECV_STYLE_BROADCAST 0x1

// replies will be sent via unicast
#define DHCP_RECV_STYLE_UNICAST 0x0

// must be added before all options
#define DHCP_MAGIC_COOKIE 0x63825363

// passed into dhcp_write_header as the gateway address if dhcp server and client are on the same subnet
#define DHCP_SERVER_SAME_SUBNET 0

#define DHCP_HDR_RET_SUCCESS 0x0
#define DHCP_HDR_RET_INVALID_OPTIONS 0x1

#define DHCP_SET_BROADCAST (1u << 15)

typedef enum {
    OPT_PAD,
    OPT_SUBNET_MASK,
    OPT_TIME_OFFSET,
    OPT_ROUTER,
    OPT_TIME_SERVER,
    OPT_NAME_SERVER,
    OPT_DNS,
    OPT_LOG_SERVER,
    OPT_COOKIE_SERVER,
    OPT_LPR_SERVER,
    OPT_IMPRESS_SERVER,
    OPT_RLP_SERVER,
    OPT_HOST_NAME,
    OPT_BOOT_FILE_SIZE,
    OPT_MERIT_DUMP_FILE,
    OPT_DOMAIN_NAME,
    OPT_SWAP_SERVER,
    OPT_ROOT_PATH,
    OPT_EXTENSIONS_PATH,

    // dhcp extention options
    OPT_REQUESTED_IP_ADDR = 50,
    OPT_ADDR_LEASE_TIME,
    OPT_OVERLOAD,
    OPT_DHCP_MSG_TYPE,
    OPT_SERVER_IDENT,
    OPT_PARAM_REQ_LIST,
    OPT_MSG,

    OPT_END = 0xff,
}dhcp_option_e;

typedef enum {
    DHCPDISCOVER = 1,
    DHCPOFFER,
    DHCPREQUEST,
    DHCPDECLINE,
    DHCPACK,
    DHCPNAK,
    DHCPRELEASE,
    DHCPINFORM
}dhcp_msg_type_e;

/**
 * allocating a network address:
 * Client: broadcasts DHCPDISCOVER
 * each server response with an DHCPOFFER in your_ip_addr
 * Client picks one offer with DHCPREQUEST to request that ip addr (broadcast with selected server as option 'server identifier')
 * server acknowledges using DHCPACK
 * 
 * Client should: do an ARP request against returned address. that should not return something, since then it would be already in use
 * store length of lease and renew when needed
 */

typedef struct {
    uint8_t op_code;
    uint8_t htype;
    uint8_t hlen;
    uint8_t hops; // should be zero 
    uint32_t xid; // transaction id chosen by client (should be random)
    uint16_t secs; // seconds elapsed since begin of acquisition or renewal process
    uint16_t flags;
    uint32_t client_ip_addr;
    uint32_t your_ip_addr; // return field
    uint32_t server_ip_addr; // return field from DHCPOFFER and DHCPACK
    uint32_t gateway_ip_addr; // address of gateway if DHCP server and client are not on same subnet, otherwise 0
    uint8_t client_hardware_addr[16];
    uint8_t server_name[64];
    uint8_t boot_file_name[128];
    uint32_t magic_cookie;
    uint8_t options[];
}__attribute__((packed)) dhcp_header_t;

typedef struct {
    uint8_t type;
    uint8_t len;
    uint8_t data[];
}__attribute__((packed)) dhcp_option_t;

typedef struct {
    uint32_t ip_addr;
    uint32_t server_ip_addr;
    uint32_t subnet_mask;
    uint32_t router_addr;
    uint32_t lease_time;
    uint32_t dns_cnt;
    uint32_t* dns_arr;
}dhcp_option_info_t;

typedef enum{
    DHCP_INIT,
    DHCP_SELECTING,
    DHCP_REQUESTING,
    DHCP_BOUND,
    DHCP_RENEWING,
    DHCP_REBINDING,
}dhcp_state_e;

typedef struct dhcp_client{
    dhcp_state_e state;

    uint32_t xid;

    uint32_t ip_addr;
    uint32_t server;

    /**
     * Note: 
     * if time < renewal: we happy, valid lease
     * if renewal < time < rebind: we may renew by sending a dhcp request to the server (renewal at 50% possible)
     * if time >= rebind: completely repeat discover and request, server may no longer be valid (at 87.5% of lease time)
     */
    uint32_t lease_end;
    uint32_t renewal_timestamp; 
    uint32_t rebind_timestamp;
}dhcp_client_t;

struct net_interface;
typedef struct net_interface net_interface_t;

/**
 * dhcp_write_header:
 * Writes a dhcp header with options into a buffer 
 * @param iface The interface to optain the client hardware address from
 * @param data The buffer to write the header into
 * @param xid The transaction id to use for the header
 * @param recv_style Whether the server should reply via broadcast or unicast
 * @param gateway_ip_addr The gateway address to use in the header, if the server is on the same subnet as the client, this should be 0
 * @param client_ip_addr Used to renew lease, otherwise should be 0 (since we dont have an IP address yet)
 * @param server_ip_addr Used in DHCP OFFER, otherwise 0
 * @param options A queue of dhcp_option_t to write into the header, must contain an OPT_END option at the end
 * @return DHCP_HDR_RET_SUCCESS on success, DHCP_HDR_RET_INVALID_OPTIONS if options is null or does not contain an OPT_END option
 */
uint8_t dhcp_write_header(net_interface_t* iface, uint8_t* data, uint32_t xid, uint8_t recv_style, uint32_t gateway_ip_addr, uint32_t client_ip_addr, uint32_t server_ip_addr, queue_t* options);

/**
 * dhcp_configure_ip:
 * Configures the ip address of the kernel by sending a dhcp discover and request to the network, and waiting for an offer and ack from the server
 */
void dhcp_configure_ip();
#endif