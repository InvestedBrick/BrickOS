#include "ip.h"
#include "networking.h"
#include "../memory/kmalloc.h"
#include "ethernet.h"
#include "../utilities/util.h"
#include "../io/log.h"
#include "../tables/interrupts.h"
#include <stdatomic.h>

static atomic_uint_fast16_t ip_id;
ipv4_ll_link_t* packet_ll_origin = nullptr;
mutex_t ip_ll_mutex;

uint64_t create_packet_part_ident(uint32_t src_ip, uint16_t ident, uint8_t protocol){
    return ((uint64_t)src_ip | ((uint64_t)ident << 32) | ((uint64_t)protocol << 48));
}

ipv4_ll_link_t* find_ipv4_packet_start_link(uint64_t ident){
    ipv4_ll_link_t* curr = packet_ll_origin;
    while(curr && curr->start && curr->start->ident != ident) curr = curr->next;

   return curr ? curr : nullptr;
}


void abort_reassembly(ipv4_packet_part_t* part){
    if (!part) return;

    ipv4_ll_link_t* link = find_ipv4_packet_start_link(part->ident);
    if (!link) return;  // nothing to remove

    ipv4_ll_link_t* prev = packet_ll_origin;
    if (prev == link) packet_ll_origin = link->next;
    else {
        while (prev && prev->next != link) prev = prev->next;
        prev->next = link->next;
    }

    ipv4_packet_part_t* curr = link->start;
    while (curr) {
        ipv4_packet_part_t* next = curr->next;
        kfree(curr->data);
        kfree(curr);
        curr = next;
    }

    kfree(link);
}

void ipv4_timer_callback(){
    // called every second
    mutex_wait(&ip_ll_mutex,TIMOUT_INF);
    ipv4_ll_link_t* head = packet_ll_origin;
    while(head){
        ipv4_ll_link_t* to_del = nullptr;
        head->timeout--;
        if (head->timeout == 0) {
            to_del = head;
        }
        head = head->next;
        if (to_del){
            abort_reassembly(to_del->start);
            
        }
    }
    mutex_signal(&ip_ll_mutex);
}

route_t* route_lookup(uint32_t dst_ip){
    for (uint32_t i = 0; i < routing_table.n_routes;i++){
        route_t* route = &routing_table.routes[i];
        if ((dst_ip & route->netmask) == (route->network & route->netmask)) return route;
    }
    return nullptr;
}

void insert_ipv4_packet_part(ipv4_packet_part_t* part){
    if (!part) return;

    ipv4_ll_link_t* ll_start = find_ipv4_packet_start_link(part->ident);
    if (!ll_start){
        // part is first with specific ident
        ipv4_ll_link_t* ll_link = (ipv4_ll_link_t*)kmalloc(sizeof(ipv4_ll_link_t));
        ll_link->start = part;
        ll_link->timeout = IP_PACKET_TIMEOUT;
        ll_link->next = nullptr;

        if (!packet_ll_origin) packet_ll_origin = ll_link;
        else{
            ipv4_ll_link_t* prev = packet_ll_origin;
            while(prev->next) prev = prev->next;
            prev->next = ll_link;
        }
        return;
    }

    // insert part into linked list based on fragment offset
    if (!ll_start->start) panic("Corrupted IP packet linked list"); // should not be able to happen

    ipv4_packet_part_t** curr = &ll_start->start;
    while (*curr && (*curr)->frag_offset < part->frag_offset)
        curr = &(*curr)->next;

    part->next = *curr;
    *curr = part;

}

uint8_t ipv4_packet_complete(ipv4_packet_part_t* part){
    ipv4_packet_part_t* start = find_ipv4_packet_start_link(part->ident)->start; // assumed safe because insert must have been called before

    ipv4_packet_part_t* curr = start;
    
    uint32_t expected_offset = 0;
    ipv4_packet_part_t* last = nullptr;
    while(curr){
        if (curr->frag_offset * 8 != expected_offset) return 0; // no perfect tiling
        expected_offset += curr->data_len; 
        last = curr; 
        curr = curr->next;
    }
    
    if (!last || !last->no_more_frags) return 0;
    
    uint32_t total_packet_size = last->frag_offset * 8 + last->data_len; // frag offsets are in units of 8 bytes
    
    return (expected_offset == total_packet_size);

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

    if (len + IP_HDR_DEFAULT_SIZE > route->iface->mtu) return IP_SEND_RET_MTU_OVERSTEP;

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

uint32_t unify_ip_packet(ipv4_packet_part_t* part, uint8_t** out_data){
    ipv4_ll_link_t* link = find_ipv4_packet_start_link(part->ident);
    if (!link) panic("No ipv4 packet linked list link was found, should have been there"); // should never run
    if (!link->start) panic("No ipv4 packet linked list start was found, should have been there");

    ipv4_packet_part_t* packet_start = link->start;

    // unlink from larger linked list
    ipv4_ll_link_t* prev = packet_ll_origin;
    if (prev == link) packet_ll_origin = link->next;
    else{
        while(prev->next != link) prev = prev->next;
        prev->next = link->next;
    }

    kfree(link);

    
    ipv4_packet_part_t* curr = packet_start;
    while(curr->next) curr = curr->next;

    uint32_t total_packet_size = curr->frag_offset * 8 + curr->data_len;

    *out_data = (uint8_t*)kmalloc(total_packet_size);

    curr = packet_start;
    while (curr) {
        memcpy(*out_data + curr->frag_offset * 8, curr->data, curr->data_len);
        ipv4_packet_part_t* del = curr;
        curr = curr->next;
        kfree(del->data);
        kfree(del);
    }

    return total_packet_size;
}

void hand_ip_packet_along(uint8_t* data, uint32_t len,uint8_t protocol){
    switch (protocol)
    {
    case IP_PROTOCOL_ICMP:
        /* code */
        break;
    case IP_PROTOCOL_TCP:
        break;
    case IP_PROTOCOL_UDP:
        break;
    case IP_PROTOCOL_RAW:
        break;
    default:
        warnf("recieved unhandled protocol (%d)", protocol);
        break;
    }
}

void ip_handle_packet(uint8_t* data, uint32_t write_off, uint32_t total_len) {
    ipv4_header_t* ipv4_hdr = (ipv4_header_t*)(data + write_off);

    uint8_t version = (ipv4_hdr->version_ihl >> 4) & 0xf;
    if (version != IP_VERSION_4) return;

    uint32_t dst_ip = switch_endian32(ipv4_hdr->dst_ip);

    route_t* route = route_lookup(dst_ip);
    if (!route) return; 

    if (dst_ip != route->iface->ip_addr) return; // not for me ; TODO: add acceptance for multicast stuff

    uint32_t hdr_len = (ipv4_hdr->version_ihl & 0xf) * sizeof(uint32_t);
    
    if (hdr_len < 5 * sizeof(uint32_t)) return; // header too small
    if (total_len < write_off + hdr_len) return; // header too large

    uint16_t total_packet_part_length = switch_endian16(ipv4_hdr->total_length);
    if (total_packet_part_length < hdr_len) return;
    if (total_len < write_off + total_packet_part_length) return; // claimed size exceeds what we actually received

    if (compute_checksum((uint8_t*)ipv4_hdr,hdr_len) != 0) return;

    uint32_t flags_frag_off = switch_endian16(ipv4_hdr->flags_fragment_offset);

    uint16_t ident = switch_endian16(ipv4_hdr->ident);
    uint16_t post_hdr_data_len = total_packet_part_length - hdr_len;
    uint16_t frag_off = flags_frag_off & IP_FRAG_OFF_MASK;

    if (frag_off * 8 + post_hdr_data_len > IPv4_MAX_PACKET_SIZE) return;

    if (!(flags_frag_off & IP_FLAGS_MORE_FRAGMENTS) && frag_off == 0){
        // unfragmented packet
        hand_ip_packet_along(data + write_off + hdr_len,post_hdr_data_len,ipv4_hdr->protocol);
    }else{
        //fragmented

        ipv4_packet_part_t* part = (ipv4_packet_part_t*)kmalloc(sizeof(ipv4_packet_part_t));
        uint8_t* post_hdr_data = (uint8_t*)kmalloc(post_hdr_data_len);
        memcpy(post_hdr_data,(void*)(data + write_off + hdr_len), post_hdr_data_len);
        uint32_t src_ip = switch_endian32(ipv4_hdr->src_ip);
        part->ident = create_packet_part_ident(src_ip,ident,ipv4_hdr->protocol);
        part->data_len = post_hdr_data_len;
        part->data = post_hdr_data;
        part->protocol = ipv4_hdr->protocol;

        part->frag_offset = frag_off;
        part->no_more_frags = (flags_frag_off & IP_FLAGS_MORE_FRAGMENTS) == 0;

        part->next = nullptr;
        
        mutex_wait(&ip_ll_mutex,TIMOUT_INF);
        insert_ipv4_packet_part(part);

        if (ipv4_packet_complete(part)){
            uint8_t* out_data;
            uint32_t len = unify_ip_packet(part,&out_data);
            hand_ip_packet_along(out_data,len,part->protocol);
            kfree(out_data); // can be freed here since was allocated in unify_ip_packet
        }
        mutex_signal(&ip_ll_mutex);


    }


}