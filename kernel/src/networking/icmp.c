#include "icmp.h"
#include "networking.h"
#include <shared/util.h>
#include "ip.h"
#include "ethernet.h"
#include "../memory/kmalloc.h"
#include "../io/log.h"
#include <stdatomic.h>

static atomic_uint_fast16_t icmp_id;

icmp_socket_t* icmp_sock_head = nullptr;
mutex_t icmp_sock_queue_lock;

uint16_t get_new_icmp_ident(){
    return atomic_fetch_add(&icmp_id,1);
}

uint16_t get_true_icmp_header_size(uint8_t icmp_type){
    uint16_t size = DEFAULT_ICMP_HDR_SIZE;
    switch (icmp_type)
    {
    case ICMP_TYPE_TIMESTMP_MSG:
    case ICMP_TYPE_TIMESTMP_REPLY:
        size += sizeof(icmp_timestamp_t) - sizeof(uint32_t); // one uint32 is already included
        break;
    
    default:
        break;
    }
    return size;
}
void icmp_add_extra_payload(uint8_t* data, uint32_t* write_off, uint8_t* payload, uint32_t payload_len){
    *write_off -= payload_len;
    memcpy(data + *write_off, payload, payload_len);
}

uint8_t icmp_add_hdr(uint8_t* data, uint32_t* write_off, uint8_t icmp_type, uint8_t icmp_code, uint32_t may_be_used_dword, uint8_t* extra_payload, uint32_t extra_payload_len){
    uint16_t true_size = get_true_icmp_header_size(icmp_type);
    if (*write_off < true_size + extra_payload_len) return ICMP_HDR_RET_DATA_OVERFLOW;
    
    if (extra_payload && extra_payload_len){
        icmp_add_extra_payload(data, write_off, extra_payload, extra_payload_len);
    }

    *write_off -= true_size;

    icmp_header_t* hdr = (icmp_header_t*)(data + *write_off);
    memset((void*)hdr,0x0, true_size); // saves the hassle to set unused fields or many icmp_codes to 0
    void* post_hdr = (void*)((uint8_t*)hdr + sizeof(icmp_header_t));
    switch (icmp_type){
        case ICMP_TYPE_ECHO_REPLY:
        case ICMP_TYPE_ECHO_MSG:;
            icmp_echo_t* echo = (icmp_echo_t*)post_hdr;
            echo->echo_ident = switch_endian16((uint16_t)(may_be_used_dword >> 16));
            echo->echo_seq = switch_endian16((uint16_t)(may_be_used_dword & 0xffff));
            break;
        case ICMP_TYPE_REDIR_MSG:;
            icmp_redir_t* redir = (icmp_redir_t*)post_hdr;
            redir->gateway_ip_addr = switch_endian32(may_be_used_dword);
            break;
        case ICMP_TYPE_PARAM_PRBLM_MSG:;
            icmp_param_problem_t* param_problem = (icmp_param_problem_t*)post_hdr;
            param_problem->ptr = (uint8_t)(may_be_used_dword & 0xff);
            break;
        case ICMP_TYPE_TIMESTMP_MSG:
        case ICMP_TYPE_TIMESTMP_REPLY:;
            icmp_timestamp_t* timestamp = (icmp_timestamp_t*)post_hdr;
            timestamp->time_stmp_ident = switch_endian16((uint16_t)(may_be_used_dword >> 16));
            timestamp->time_stmp_seq = switch_endian16((uint16_t)(may_be_used_dword & 0xffff));
            break;
        case ICMP_TYPE_INFO_REQ_MSG:
        case ICMP_TYPE_INFO_REPLY_MSG:;
            icmp_info_t* info = (icmp_info_t*)post_hdr;
            info->info_ident = switch_endian16((uint16_t)(may_be_used_dword >> 16));
            info->info_seq = switch_endian16((uint16_t)(may_be_used_dword & 0xffff));
            hdr->icmp_code = 0;
            break;
        default:
            return ICMP_HDR_RET_INVALID_TYPE;
            break;
    }
    hdr->icmp_type = icmp_type;
    hdr->icmp_code = icmp_code;
    hdr->checksum  = 0;
    hdr->checksum = switch_endian16(compute_checksum((uint8_t*)hdr, true_size + extra_payload_len));


    return ICMP_HDR_RET_SUCCESS;
}

void icmp_handle_packet(uint8_t* data, uint32_t len){
    if (len < DEFAULT_ICMP_HDR_SIZE) return;

    ipv4_header_t* ip_hdr = (ipv4_header_t*)(data);
    uint8_t ip_hlen = (ip_hdr->version_ihl & 0xf) * sizeof(uint32_t);
    uint32_t post_ip_hdr_data_len = len - ip_hlen;

    icmp_header_t* icmp_hdr = (icmp_header_t*)(data + ip_hlen);
    if (compute_checksum((uint8_t*)icmp_hdr, post_ip_hdr_data_len) != 0) return;

    switch (icmp_hdr->icmp_type)
    {
    case ICMP_TYPE_ECHO_REPLY:
    case ICMP_TYPE_TIMESTMP_REPLY:
        // stuff to just send to every ICMP socket
        icmp_handle_socket_packet(ip_hdr,ip_hlen, len);
        break;
    case ICMP_TYPE_DEST_UNR_MSG:
        icmp_handle_dest_unreachable(ip_hdr,ip_hlen, len);
        break;
    case ICMP_TYPE_SRC_QUENCH_MSG:
        icmp_handle_source_quench(ip_hdr,ip_hlen, len);
        break;
    case ICMP_TYPE_REDIR_MSG:
        icmp_handle_redirect(ip_hdr,ip_hlen, len);
        break;
    case ICMP_TYPE_ECHO_MSG:
        icmp_handle_echo_request(ip_hdr,ip_hlen, len);
        break;
    case ICMP_TYPE_TIME_EXC_MSG:
        icmp_handle_time_exceeded(ip_hdr,ip_hlen, len);
        break;
    case ICMP_TYPE_PARAM_PRBLM_MSG:
        icmp_handle_parameter_problem(ip_hdr,ip_hlen, len);
        break;
    case ICMP_TYPE_TIMESTMP_MSG:
        icmp_handle_timestamp(ip_hdr,ip_hlen, len);
        break;
    case ICMP_TYPE_INFO_REQ_MSG:
        icmp_handle_info_request(ip_hdr, ip_hlen, len);
        break;
    case ICMP_TYPE_INFO_REPLY_MSG:
        icmp_handle_info_reply(ip_hdr,ip_hlen, len);
        break;
    default:
        break;
    }

}

void icmp_handle_socket_packet(ipv4_header_t* ip_hdr, uint8_t ip_hlen, uint32_t total_len){
    mutex_wait(&icmp_sock_queue_lock,LOCK_TIMEOUT_INF);

    icmp_socket_t* sock = icmp_sock_head;
    if (!sock) {
        mutex_signal(&icmp_sock_queue_lock);
        return;
    }

    refcnt_packet_t* packet = (refcnt_packet_t*)kmalloc(sizeof(refcnt_packet_t));
    uint8_t* data_buffer = (uint8_t*)kmalloc(total_len);
    memcpy(data_buffer,(void*)ip_hdr,total_len);
    packet->refcnt = 0;
    packet->packet.data = data_buffer;
    packet->packet.data_len = total_len;
    packet->packet.next = nullptr;
    packet->packet.src_addr.inet_addr = switch_endian32(ip_hdr->src_ip);
    packet->packet.src_addr.inet_port = 0;
    packet->packet.src_addr.inet_family = INET_FAM_IPv4;

    while(sock){
        mutex_wait(&sock->sock.lock,LOCK_TIMEOUT_INF);
        atomic_fetch_add(&packet->refcnt,1);
        enqueue_rx_data((generic_proto_socket_t*)sock,(recvd_packet_t*)packet);
        mutex_signal(&sock->sock.lock);
        sock = (icmp_socket_t*)sock->sock.next;
    }

    mutex_signal(&icmp_sock_queue_lock);
}

void icmp_handle_dest_unreachable(ipv4_header_t* ip_hdr, uint8_t ip_hlen, uint32_t total_len){
    // WIP
}

void icmp_handle_source_quench(ipv4_header_t* ip_hdr, uint8_t ip_hlen, uint32_t total_len){
    // OBSOLETE
}

void icmp_handle_redirect(ipv4_header_t* ip_hdr,uint8_t ip_hlen,  uint32_t total_len){
    icmp_header_t* icmp_hdr = (icmp_header_t*)((uint8_t*)ip_hdr + ip_hlen);

    uint16_t true_size = get_true_icmp_header_size(ICMP_TYPE_REDIR_MSG);
    ipv4_header_t* inner_ip_hdr = (ipv4_header_t*)((uint8_t*)icmp_hdr + true_size);
    
    if (total_len < true_size + sizeof(ipv4_header_t)) return;
    
    uint32_t original_dest_ip = switch_endian32(inner_ip_hdr->dst_ip);
    
    if (!original_dest_ip) return;

    icmp_redir_t* redir = (icmp_redir_t*)((uint8_t*)icmp_hdr + sizeof(icmp_header_t));
    uint32_t new_gateway = switch_endian32(redir->gateway_ip_addr);

    route_t* route = route_lookup(original_dest_ip);

    if (!route) return;

    uint32_t src_ip = switch_endian32(ip_hdr->src_ip);

    if (route->gateway != src_ip) return; // ignore redirects that are not from the gateway

    switch (icmp_hdr->icmp_code)
    {
    case ICMP_REDIR_NETWORK:
    case ICMP_REDIR_TOS_NETWORK:
        // updated for all destinations in target network
        update_or_insert_route(route->iface, original_dest_ip & route->netmask, route->netmask, new_gateway);
        break;
    case ICMP_REDIR_HOST:
    case ICMP_REDIR_TOS_HOST:
        // only reroute this specific destination
        update_or_insert_route(route->iface,original_dest_ip,0xffffffff,new_gateway);
        break;
    default:
        return;
    }

}
void icmp_handle_echo_request(ipv4_header_t* ip_hdr, uint8_t ip_hlen, uint32_t total_len){
    icmp_header_t* icmp_hdr = (icmp_header_t*)((uint8_t*)ip_hdr + ip_hlen);
    uint32_t src_ip = switch_endian32(ip_hdr->src_ip);

    icmp_echo_t* echo = (icmp_echo_t*)((uint8_t*)icmp_hdr + sizeof(icmp_header_t));
    uint16_t ident = switch_endian16(echo->echo_ident);
    uint16_t seq = switch_endian16(echo->echo_seq);

    uint16_t true_size = get_true_icmp_header_size(ICMP_TYPE_ECHO_MSG);

    icmp_send_data_t* send_data = kmalloc(sizeof(icmp_send_data_t));
    send_data->icmp_code = 0;
    send_data->icmp_type = ICMP_TYPE_ECHO_REPLY;
    send_data->may_be_used = ((uint32_t)ident << 16) | seq;
    send_data->extra_payload = (uint8_t*)icmp_hdr + true_size;
    send_data->extra_payload_len = total_len - ip_hlen - true_size;

    socket_t fake_sock;
    fake_sock.ip_opts.hdr_incl = 0;
    fake_sock.ip_opts.tos = IP_TOS_DEFAULT;
    fake_sock.ip_opts.ttl = IP_TTL_MAX;

    send_ip_based_packet(&fake_sock,
                         nullptr,
                         0,
                         src_ip,
                         IP_PROTOCOL_ICMP,
                         send_data);

    kfree(send_data);
}

void icmp_handle_time_exceeded(ipv4_header_t* ip_hdr, uint8_t ip_hlen, uint32_t total_len){
    // WIP
}

void icmp_handle_parameter_problem(ipv4_header_t* ip_hdr, uint8_t ip_hlen, uint32_t total_len){
    // WIP
}

void icmp_handle_timestamp(ipv4_header_t* ip_hdr, uint8_t ip_hlen, uint32_t total_len){
    // WIP
}

void icmp_handle_info_request(ipv4_header_t* ip_hdr,uint8_t ip_hlen, uint32_t total_len){
    // OBSOLETE
}

void icmp_handle_info_reply(ipv4_header_t* ip_hdr,uint8_t ip_hlen, uint32_t total_len){
    // OBSOLETE
}

int icmp_sendto(socket_t* sock, void* buf, uint32_t buf_len, uint32_t flags, sockaddr_t* dst_addr, uint32_t addr_len){
    icmp_socket_t* icmp_sock = (icmp_socket_t*)sock->prot_sock;
    in_sockaddr_t* in_addr = (in_sockaddr_t*)dst_addr;

    if (addr_len != sizeof(in_sockaddr_t)) return RAW_IP_RET_FAIL;

    if (send_ip_based_packet(sock,buf,buf_len,in_addr->inet_addr,IP_PROTOCOL_ICMP,nullptr) != IP_SEND_RET_SUCCESS)
        return ICMP_RET_FAIL;

    return buf_len;
}

int icmp_recvfrom(socket_t* sock, void* buf, uint32_t buf_len, uint32_t flags, sockaddr_t* src_addr, uint32_t* addr_len){
    return generic_inet_recvfrom(sock,buf,buf_len,flags,src_addr,addr_len,true);
}


void icmp_cleanup_sock(generic_proto_socket_t* sock){
    cleanup_socket((generic_proto_socket_t**)&icmp_sock_head,sock,&icmp_sock_queue_lock);
}
proto_handles_t icmp_proto_handles = {
    .bind = 0, // not needed rn
    .sendto = icmp_sendto,
    .recvfrom = icmp_recvfrom
};