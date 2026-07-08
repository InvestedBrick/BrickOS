#include "ethernet.h"
#include "networking.h"
#include "../memory/kmalloc.h"
#include "../utilities/util.h"

void ethernet_handle_packet(uint8_t* data, uint32_t len){
    if (len < sizeof(ethernet_header_t)) return;
    ethernet_header_t* eth_hdr = (ethernet_header_t*)data; 
    uint16_t type = switch_endian16(eth_hdr->type);
    switch (type)
    {
    case ETHERTYPE_ARP:
        
        break;
    case ETHERTYPE_IPv4:

        break;
    case ETHERTYPE_IPv6:

        break;
    default:
        
        break;
    }
    kfree(data);
}

uint8_t ethernet_add_header(uint8_t* data, uint32_t* curr_len,uint32_t total_len,ethernet_header_t* eth_hdr){
    if (*curr_len + sizeof(ethernet_header_t) > total_len) return ETH_HDR_RET_DATA_OVERFLOW;

    memcpy((void*)(&data[*curr_len]),eth_hdr,sizeof(uint8_t[8]) * 2); // both MAC addresses
    uint16_t type = switch_endian16(eth_hdr->type);

    ((ethernet_header_t*)(&data[*curr_len]))->type = type;
    *curr_len += sizeof(ethernet_header_t);
    return ETH_HDR_RET_SUCCESS;
}