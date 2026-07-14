#include "ip.h"
#include "networking.h"
#include "../memory/kmalloc.h"
#include "ethernet.h"
#include "../utilities/util.h"
#include "../io/log.h"
#include <stdatomic.h>

static atomic_uint_fast16_t ip_id;

route_t* route_lookup(uint32_t dst_ip){
    for (uint32_t i = 0; i < routing_table.n_routes;i++){
        route_t* route = &routing_table.routes[i];
        if ((dst_ip & route->netmask) == (route->network & route->netmask)) return route;
    }
    return nullptr;
}

uint8_t ip_add_header(net_interface_t* iface,uint8_t* data, uint32_t* write_off,uint32_t dst_addr,uint8_t prot, uint8_t tos, uint8_t ttl,uint16_t id,uint8_t df,uint32_t post_hdr_data_len){
    if (*write_off < sizeof (ipv4_header_t)) return IP_HDR_RET_DATA_OVERFLOW;
    ipv4_header_t* hdr = (ipv4_header_t*)(data + *write_off - sizeof(ipv4_header_t));


    hdr->version_ihl = (IP_VERSION_4 << 4) | (IP_HDR_DEFAULT_SIZE / sizeof(uint32_t));
    hdr->type_of_service = tos;

    hdr->ident = switch_endian16(id);
    hdr->protocol = prot;
    hdr->time_to_live = ttl;
    hdr->flags_fragment_offset = 0;
    if (df) hdr->flags_fragment_offset |= switch_endian16(IP_FLAGS_DONT_FRAGMENT);
    hdr->src_ip = switch_endian32(iface->ip_addr);
    hdr->dst_ip = switch_endian32(dst_addr);
    hdr->total_length = switch_endian16(IP_HDR_DEFAULT_SIZE + post_hdr_data_len);
    hdr->header_checksum = 0;
    hdr->header_checksum = switch_endian16(compute_checksum((uint8_t*)hdr,IP_HDR_DEFAULT_SIZE));

    *write_off -= sizeof(ipv4_header_t);
    return IP_HDR_RET_SUCCESS;
}

uint8_t send_ip_packet(uint8_t* data, uint32_t len, uint32_t dst_ip){
    
    route_t* route = route_lookup(dst_ip);
    if (!route) return IP_SEND_RET_NO_ROUTE;

    if (len > route->iface->mtu) return IP_SEND_RET_MTU_OVERSTEP;

    uint32_t next_hop = 0;

    if (route->gateway)
        next_hop = route->gateway; // outside local network
    else 
        next_hop = dst_ip; // inside local network
    
    uint8_t dst_mac[6] = {0};

    arp_lookup(next_hop,dst_mac);

    uint32_t total_header_len = sizeof(ipv4_header_t) + sizeof(ethernet_header_t);
    uint32_t write_off = total_header_len;
    uint8_t* data_buf = (uint8_t*)kmalloc(total_header_len + len);

    uint16_t id = atomic_fetch_add(&ip_id,1);

    uint8_t ret = ip_add_header(route->iface,
                                data_buf,
                                &write_off,
                                dst_ip,
                                IP_PROTOCOL_RAW,
                                IP_TOS_DEFAULT,
                                IP_TTL_MAX,
                                id,
                                IP_MAY_FRAGMENT,
                                len);
    if (ret != IP_HDR_RET_SUCCESS) {
        warnf("Failed to add IP header to IP packet (ERR: %x)",ret);
        kfree(data_buf);
        return IP_SEND_RET_IP_HDR_FAILED;
    }

    ret = ethernet_add_header(route->iface,
                              data_buf,
                              &write_off,
                              dst_mac,
                              ETHERTYPE_IPv4);
    if (ret != ETH_HDR_RET_SUCCESS) {
        warnf("Failed to add ETHERNET header to IP packet (ERR: %x)",ret);
        kfree(data_buf);
        return IP_SEND_RET_ETH_HDR_FAILED;
    }


    memcpy(&data_buf[total_header_len],data,len);

    route->iface->send(data_buf,total_header_len + len); // buffer gets distributed into pages and memcpied, so freeing after send is fine

    kfree(data_buf);

    return IP_SEND_RET_SUCCESS;
}

void ip_handle_packet(uint8_t* data, uint32_t write_off, uint32_t total_len) {

}