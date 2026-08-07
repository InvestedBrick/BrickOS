#include "ip.h"
#include "networking.h"
#include "../memory/kmalloc.h"
#include "ethernet.h"
#include "../utilities/util.h"
#include "../io/log.h"
#include "../tables/interrupts.h"
#include "icmp.h"
#include "udp.h"
#include <stdatomic.h>

static atomic_uint_fast16_t ip_id;
ipv4_ll_link_t* packet_ll_origin = nullptr;
mutex_t ip_ll_mutex;

raw_ip_socket_t* raw_ip_sock_head = nullptr;
mutex_t raw_ip_sock_lock;

void init_ip_linked_lists(){
    mutex_init(&ip_ll_mutex);
}

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
    mutex_wait(&ip_ll_mutex,LOCK_TIMEOUT_INF);
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
    route_t* best = nullptr;
    for (uint32_t i = 0; i < routing_table.n_routes;i++){
        route_t* r = &routing_table.routes[i];
        if ((dst_ip & r->netmask) == r->network){
            if (!best || r->netmask > best->netmask){
                best = r;
            }
        }
    }
    return best;
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

uint8_t send_ip_based_packet(socket_t* sock,uint8_t* usr_data, uint32_t usr_data_len, uint32_t dst_ip, uint8_t higher_prot, void* higher_prot_data){
    route_t* route = route_lookup(dst_ip);
    if (!route) return IP_SEND_RET_NO_ROUTE;

    uint32_t total_hdr_len = sizeof(ethernet_header_t) + sizeof(ipv4_header_t);
    switch (higher_prot)
    {
    case IP_PROTOCOL_ICMP:;
        icmp_send_data_t* icmp_send = (icmp_send_data_t*)higher_prot_data;
        uint16_t icmp_hdr_size = get_true_icmp_header_size(icmp_send->icmp_type);
        total_hdr_len += icmp_hdr_size;
        total_hdr_len += icmp_send->extra_payload_len;
        break;
    case IP_PROTOCOL_UDP:
        total_hdr_len += sizeof(udp_header_t);
        break;
    
    default:
        break;
    }

    if (usr_data_len + total_hdr_len > route->iface->mtu) return IP_SEND_RET_MTU_OVERSTEP;

    uint32_t next_hop = 0;

    if (route->gateway)
        next_hop = route->gateway; // outside local network
    else 
        next_hop = dst_ip; // inside local network
    
    uint8_t dst_mac[6] = {0};

    arp_lookup(next_hop,dst_mac);

    uint32_t write_off = total_hdr_len;
    uint8_t* data_buf = (uint8_t*)kmalloc(total_hdr_len + usr_data_len);

    uint16_t id = atomic_fetch_add(&ip_id,1);
    
    uint32_t post_hdr_len = usr_data_len;
    uint8_t ret = 0;
    if (higher_prot == IP_PROTOCOL_ICMP){
        icmp_send_data_t* icmp_send = (icmp_send_data_t*)higher_prot_data;
        uint16_t icmp_hdr_size = get_true_icmp_header_size(icmp_send->icmp_type);
        post_hdr_len += icmp_hdr_size + icmp_send->extra_payload_len;

        ret = icmp_add_hdr(data_buf,
                           &write_off,
                           icmp_send->icmp_type,
                           icmp_send->icmp_code,
                           icmp_send->may_be_used,
                           icmp_send->extra_payload,
                           icmp_send->extra_payload_len);

        if (ret != ICMP_HDR_RET_SUCCESS) {
            warnf("Failed to add ICMP header to IP based packet (ERR:%x)",ret);
            kfree(data_buf);
            return IP_SEND_RET_ICMP_HDR_FAILED;
        }
    } else if (higher_prot == IP_PROTOCOL_UDP){
        udp_send_data_t* udp_send = (udp_send_data_t*)higher_prot_data;
        post_hdr_len += sizeof(udp_header_t);

        ret = udp_add_header(route->iface,
                             data_buf,
                             &write_off,
                             udp_send->src_port,
                             udp_send->dst_port,
                             usr_data_len,
                             usr_data,
                             dst_ip);
        if (ret != UDP_HDR_RET_SUCESS) {
            warnf("Failed to add UDP header to IP based packet (ERR: %x)",ret);
            kfree(data_buf);
            return IP_SEND_RET_UDP_HDR_FAILED;
        }
    }

    if (!sock->ip_opts.hdr_incl){
        ret = ip_add_header(route->iface,
            data_buf,
            &write_off,
            dst_ip,
            higher_prot,
            sock->ip_opts.tos,
            sock->ip_opts.ttl,
            id,
            IP_MAY_FRAGMENT,
            post_hdr_len);
            if (ret != IP_HDR_RET_SUCCESS) {
                warnf("Failed to add IP header to IP packet (ERR: %x)",ret);
                kfree(data_buf);
                return IP_SEND_RET_IP_HDR_FAILED;
            }
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

    if (usr_data){
        memcpy(&data_buf[total_hdr_len],usr_data,usr_data_len);
    }

    route->iface->send(data_buf,total_hdr_len + usr_data_len); // buffer gets distributed into pages and memcpied, so freeing after send is fine
    
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

void hand_ip_packet_along(uint8_t* data, uint32_t len,uint8_t protocol, uint32_t src_ip, uint32_t dst_ip){
    switch (protocol)
    {
    case IP_PROTOCOL_ICMP:
        icmp_handle_packet(data,len,src_ip);
        break;
    case IP_PROTOCOL_TCP:
        break;
    case IP_PROTOCOL_UDP:
        udp_handle_packet(data,len,src_ip,dst_ip);
        break;
    case IP_PROTOCOL_RAW1:
    case IP_PROTOCOL_RAW2:
        handle_raw_ip_packet(data,len,src_ip,dst_ip,protocol);
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
    
    uint32_t src_ip = switch_endian32(ipv4_hdr->src_ip);
    uint32_t dst_ip = switch_endian32(ipv4_hdr->dst_ip);

    route_t* route = route_lookup(src_ip); // src_ip is the IP of where it came from -> other way around should be same interface
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
        hand_ip_packet_along(data + write_off + hdr_len,post_hdr_data_len,ipv4_hdr->protocol, src_ip,dst_ip);
    }else{
        //fragmented

        ipv4_packet_part_t* part = (ipv4_packet_part_t*)kmalloc(sizeof(ipv4_packet_part_t));
        uint8_t* post_hdr_data = (uint8_t*)kmalloc(post_hdr_data_len);
        memcpy(post_hdr_data,(void*)(data + write_off + hdr_len), post_hdr_data_len);
        part->ident = create_packet_part_ident(src_ip,ident,ipv4_hdr->protocol);
        part->data_len = post_hdr_data_len;
        part->data = post_hdr_data;
        part->protocol = ipv4_hdr->protocol;

        part->frag_offset = frag_off;
        part->no_more_frags = (flags_frag_off & IP_FLAGS_MORE_FRAGMENTS) == 0;

        part->next = nullptr;
        
        mutex_wait(&ip_ll_mutex,LOCK_TIMEOUT_INF);
        insert_ipv4_packet_part(part);

        if (ipv4_packet_complete(part)){
            uint8_t* out_data;
            uint32_t len = unify_ip_packet(part,&out_data);
            hand_ip_packet_along(out_data,len,part->protocol,src_ip,dst_ip);
            kfree(out_data); // can be freed here since was allocated in unify_ip_packet
        }
        mutex_signal(&ip_ll_mutex);


    }


}

void handle_raw_ip_packet(uint8_t* data, uint32_t len, uint32_t src_ip, uint32_t dst_ip, uint8_t protocol){
    mutex_wait(&raw_ip_sock_lock,LOCK_TIMEOUT_INF);
    raw_ip_socket_t* sock = (raw_ip_socket_t*)raw_ip_sock_head;

    raw_ip_recvd_packet_t* packet = (raw_ip_recvd_packet_t*)kmalloc(sizeof(raw_ip_recvd_packet_t));
    uint8_t* data_buffer = (uint8_t*)kmalloc(len);
    memcpy(data_buffer,data,len);
    packet->packet.next = nullptr;
    packet->packet.data = data_buffer;
    packet->packet.data_len = len;
    packet->src_addr = src_ip;
    packet->protocol = protocol;
    packet->refcnt = 0;
    
    uint8_t found_any_socket = 0;
    while(sock){
        // every socket with the same protocol and either the same ip or INADDR_ANY should get the packet
        if ((sock->ip_addr == dst_ip && sock->protocol == protocol) || 
        (sock->ip_addr == INADDR_ANY && sock->protocol == protocol)) {
            mutex_wait(&sock->sock.lock,LOCK_TIMEOUT_INF);
            found_any_socket = 1;
            atomic_fetch_add(&packet->refcnt, 1);
            enqueue_rx_data((generic_proto_socket_t*)sock, (recvd_packet_t*)packet);
            mutex_signal(&sock->sock.lock);
        }
        sock = (raw_ip_socket_t*)sock->sock.next;
    }


    mutex_signal(&raw_ip_sock_lock);
    if (!found_any_socket){
        kfree(data_buffer);
        kfree(packet);
    }
    

}

void raw_ip_cleanup_sock(generic_proto_socket_t* sock){
    cleanup_socket((generic_proto_socket_t**)&raw_ip_sock_head,sock,&raw_ip_sock_lock);
}

int raw_ip_sendto(socket_t* sock, void* buf, uint32_t buf_len, uint32_t flags, sockaddr_t* dst_addr, uint32_t addr_len){

    raw_ip_socket_t* ip_sock = (raw_ip_socket_t*)sock->prot_sock;
    in_sockaddr_t* in_addr = (in_sockaddr_t*)dst_addr;

    if (addr_len != sizeof(in_sockaddr_t)) return RAW_IP_RET_FAIL;

    if (send_ip_based_packet(sock,buf,buf_len,in_addr->inet_addr,ip_sock->protocol,nullptr) != IP_SEND_RET_SUCCESS)
        return RAW_IP_RET_FAIL;

    return buf_len;
}

int raw_ip_recvfrom(socket_t* sock, void* buf, uint32_t buf_len, uint32_t flags, sockaddr_t* src_addr, uint32_t addr_len){

    raw_ip_socket_t* ip_sock = (raw_ip_socket_t*)sock->prot_sock;
    in_sockaddr_t* in_addr = (in_sockaddr_t*)src_addr;

    raw_ip_recvd_packet_t* packet =  nullptr;
    
    mutex_wait(&ip_sock->sock.lock,LOCK_TIMEOUT_INF);
    while(true){
        packet = (raw_ip_recvd_packet_t*)ip_sock->sock.rx_queue;

        if (packet) break;

        if (flags & MSG_DONTWAIT) return RAW_IP_RET_FAIL;

        add_packet_waiting_thread((generic_proto_socket_t*)ip_sock,get_current_thread(),sock->sock_opts.recv_timeout); // awoken when message arrives
        mutex_signal(&ip_sock->sock.lock);
        
        flags |= MSG_DONTWAIT; // if thread wakes up without packet ( timed out ) then it will be caught by check above 
        invoke_scheduler();

        mutex_wait(&ip_sock->sock.lock,LOCK_TIMEOUT_INF);
    }

    if (in_addr && addr_len == sizeof(in_sockaddr_t)){
        in_addr->inet_family = INET_FAM_IPv4;
        in_addr->inet_port = 0;
        in_addr->inet_addr = packet->src_addr;
    }

    uint32_t copy_len = min(buf_len,packet->packet.data_len);
    memcpy(buf,packet->packet.data,copy_len);

    if ( (!(flags & MSG_PEEK)) && atomic_fetch_sub(&packet->refcnt,1) == 1){
        erase_packet_from_rx_queue((generic_proto_socket_t*)ip_sock, (recvd_packet_t*)packet);
    }

    mutex_signal(&ip_sock->sock.lock);
    return copy_len;
}

int raw_ip_bind(socket_t* sock, sockaddr_t* addr, uint32_t len){
    raw_ip_socket_t* raw_ip_sock = (raw_ip_socket_t*)sock->prot_sock;

    if (len != sizeof(in_sockaddr_t)) return RAW_IP_RET_FAIL;
    in_sockaddr_t* inet_sockaddr = (in_sockaddr_t*)addr;
    if (inet_sockaddr->inet_family != INET_FAM_IPv4) return RAW_IP_RET_FAIL;

    mutex_wait(&raw_ip_sock->sock.lock,LOCK_TIMEOUT_INF);

    socket_clear_rx_queue((generic_proto_socket_t*)raw_ip_sock);
    socket_clear_wait_queue((generic_proto_socket_t*)raw_ip_sock);

    raw_ip_sock->ip_addr = inet_sockaddr->inet_addr;

    mutex_signal(&raw_ip_sock->sock.lock);

    return 0;
}

proto_handles_t raw_ip_proto_handles = {
    .bind = raw_ip_bind,
    .sendto = raw_ip_sendto,
    .recvfrom = raw_ip_recvfrom
};