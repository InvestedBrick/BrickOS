#include "ethernet.h"
#include "networking.h"
#include "../memory/kmalloc.h"
#include "../utilities/util.h"
#include "../io/log.h"
#include "arp.h"
#include "ip.h"

uint8_t packet_designated_for_this_machine(ethernet_header_t* eth_hdr){
    uint8_t broadcast_mac[6] = {0xff,0xff,0xff,0xff,0xff,0xff};
    if (memcmp(eth_hdr->dst_mac,broadcast_mac,sizeof(broadcast_mac)) == 0) return 1;

    net_interface_t* iface = routing_table.routes[0].iface; // entry 0 is eth0 -> has NIC MAC
    if (memcmp(eth_hdr->dst_mac,iface->mac_addr,sizeof(eth_hdr->dst_mac)) == 0) return 1;

    return 0;
}

void ethernet_handle_packet(uint8_t* data, uint32_t len){
    if (len < sizeof(ethernet_header_t)) goto cleanup;
    ethernet_header_t* eth_hdr = (ethernet_header_t*)data; 
    if (!packet_designated_for_this_machine(eth_hdr)) goto cleanup; // not for me
    uint16_t type = switch_endian16(eth_hdr->type);
    uint32_t write_off = sizeof(ethernet_header_t);
    switch (type)
    {
    case ETHERTYPE_ARP:
        arp_handle_packet(data,write_off,len);
        break;
    case ETHERTYPE_IPv4:
        ip_handle_packet(data,write_off,len);
        break;
    case ETHERTYPE_IPv6:

        break;
    default:
        warnf("Recieved network packet with unknown ethernet type");
        break;
    }
cleanup:
    kfree(data);
}

uint8_t ethernet_add_header(net_interface_t* iface,uint8_t* data, uint32_t* write_off,uint8_t* dst_mac,uint16_t ethertype){
    if (*write_off <  sizeof(ethernet_header_t)) return ETH_HDR_RET_DATA_OVERFLOW;

    ethernet_header_t* hdr = (ethernet_header_t*)(data + *write_off - sizeof(ethernet_header_t));

    memcpy(hdr->src_mac,iface->mac_addr,sizeof(hdr->src_mac));
    memcpy(hdr->dst_mac,dst_mac,sizeof(hdr->dst_mac));

    hdr->type = switch_endian16(ethertype);
    *write_off -= sizeof(*hdr);

    return ETH_HDR_RET_SUCCESS;
}