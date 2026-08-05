#include "udp.h"
#include "userspace_api/socket.h"
#include "networking.h"
#include "ip.h"
#include "../memory/kmalloc.h"
#include "../processes/scheduler.h"
#include "../io/log.h"
#include "../filesystem/vfs/vfs.h"

udp_socket_t* udp_sock_head = nullptr;
mutex_t udp_sock_queue_lock;

/**
 * POLICY:
 * udp_sock_queue_lock must only be signaled once the lock for a found socket has been aquired
 * This is to ensure that a deleted lock is not tried to be accessed
 */


uint8_t is_udp_port_used(uint16_t port){
    mutex_wait(&udp_sock_queue_lock,LOCK_TIMEOUT_INF);
    udp_socket_t* curr = udp_sock_head;
    while(curr){
        if (curr->port == port) {
            mutex_signal(&udp_sock_queue_lock);
            return 1;
        }
        curr = (udp_socket_t*)curr->sock.next;
    }
    mutex_signal(&udp_sock_queue_lock);
    return 0;

}

int udp_bind(socket_t* sock, sockaddr_t* addr, uint32_t len){
    udp_socket_t* udp_sock = (udp_socket_t*)sock->prot_sock;

    if (len != sizeof(in_sockaddr_t)) return UDP_RET_FAIL;
    in_sockaddr_t* inet_sockaddr = (in_sockaddr_t*)addr;
    if (inet_sockaddr->inet_family != INET_FAM_IPv4) return UDP_RET_FAIL;
    
    mutex_wait(&udp_sock->sock.lock,LOCK_TIMEOUT_INF);
    
    if (is_udp_port_used(inet_sockaddr->inet_port)) {
        mutex_signal(&udp_sock->sock.lock);
        return UDP_RET_FAIL;
    }

    socket_clear_rx_queue((generic_proto_socket_t*)udp_sock);
    socket_clear_wait_queue((generic_proto_socket_t*)udp_sock);

    udp_sock->ip_addr = inet_sockaddr->inet_addr;
    udp_sock->port = inet_sockaddr->inet_port;

    mutex_signal(&udp_sock->sock.lock);
    return 0;
}

int udp_sendto(socket_t* sock, void* buf, uint32_t buf_len, uint32_t flags, sockaddr_t* dst_addr, uint32_t addr_len){
    udp_socket_t* udp_sock = (udp_socket_t*)sock->prot_sock;
    in_sockaddr_t* in_addr = (in_sockaddr_t*)dst_addr;

    if (addr_len != sizeof(in_sockaddr_t)) return UDP_RET_FAIL;

    udp_send_data_t send_data;
    send_data.dst_port = in_addr->inet_port;
    send_data.src_port = udp_sock->port;

    if (send_ip_based_packet(sock,buf,buf_len,in_addr->inet_addr,IP_PROTOCOL_UDP,&send_data) != IP_SEND_RET_SUCCESS)
        return UDP_RET_FAIL;

    
    return buf_len;
}

int udp_recvfrom(socket_t* sock, void* buf, uint32_t buf_len, uint32_t flags, sockaddr_t* src_addr, uint32_t addr_len){
    
    udp_socket_t* udp_sock = (udp_socket_t*)sock->prot_sock;
    in_sockaddr_t* in_addr = (in_sockaddr_t*)src_addr;

    udp_recvd_packet_t* packet = nullptr;
    mutex_wait(&udp_sock->sock.lock,LOCK_TIMEOUT_INF);
    while(true){
        packet = (udp_recvd_packet_t*)udp_sock->sock.rx_queue;
        if (packet) break;

        if (flags & MSG_DONTWAIT) return UDP_RET_FAIL;
        mutex_signal(&udp_sock->sock.lock);
        add_packet_waiting_thread((generic_proto_socket_t*)udp_sock,get_current_thread(),sock->sock_opts.recv_timeout); // awoken when message arrives
        
        flags |= MSG_DONTWAIT;
        invoke_scheduler();
        mutex_wait(&udp_sock->sock.lock,LOCK_TIMEOUT_INF);
    }
    
    if (in_addr && addr_len == sizeof(in_sockaddr_t) ){
        in_addr->inet_family = INET_FAM_IPv4;
        in_addr->inet_addr = packet->src_addr;
        in_addr->inet_port = packet->src_port;
    }
    uint32_t copy_len = min(buf_len,packet->packet.data_len);
    memcpy(buf,packet->packet.data,copy_len);

    if (!(flags & MSG_PEEK) )
        erase_packet_from_rx_queue((generic_proto_socket_t*)udp_sock, (recvd_packet_t*)packet);

    mutex_signal(&udp_sock->sock.lock);
    return copy_len;
}

void udp_cleanup_sock(generic_proto_socket_t* sock){
    // just so that I dont have to store queue lock and queue head in the socket
    cleanup_socket((generic_proto_socket_t**)&udp_sock_head,sock,&udp_sock_queue_lock);
}

udp_socket_t* find_target_udp_socket(uint32_t ip_addr, uint16_t port){
    // must be mutex locked from outside
    udp_socket_t* curr = udp_sock_head;
    while(curr){
        if ((curr->ip_addr == ip_addr && curr->port == port) || 
            (curr->ip_addr == INADDR_ANY && curr->port == port)){
            return curr;
        }
        curr = (udp_socket_t*)curr->sock.next;
    }
    
    return nullptr;
}

void udp_enqueue_rx_data(udp_socket_t* sock, uint8_t* data, uint16_t data_len, uint32_t src_addr, uint16_t src_port){
    // sock->lock must be locked
    udp_recvd_packet_t* packet = (udp_recvd_packet_t*)kmalloc(sizeof(udp_recvd_packet_t));
    uint8_t* data_buffer = (uint8_t*)kmalloc(data_len);
    memcpy(data_buffer,data,data_len);
    packet->packet.next = nullptr;
    packet->packet.data = data_buffer;
    packet->packet.data_len = data_len;
    packet->src_addr = src_addr;
    packet->src_port = src_port;

    enqueue_rx_data((generic_proto_socket_t*)sock, (recvd_packet_t*)packet);
}

uint8_t udp_add_header(net_interface_t* iface, uint8_t* data, uint32_t* write_off, uint16_t src_port, uint16_t dst_port,uint32_t post_hdr_data_len,uint8_t* post_hdr_data, uint32_t dst_addr){
    if (*write_off < sizeof(udp_header_t)) return UDP_HDR_RET_DATA_OVERFLOW;
    udp_header_t* udp_hdr = (udp_header_t*)(data + *write_off - sizeof(udp_header_t));
    udp_hdr->checksum = 0;
    udp_hdr->src_port = switch_endian16(src_port);
    udp_hdr->dst_port = switch_endian16(dst_port);
    udp_hdr->length = switch_endian16(sizeof(udp_header_t) + post_hdr_data_len);

    pseudo_ip_hdr_t pseudo_ip;
    pseudo_ip.dst_addr = switch_endian32(dst_addr);
    pseudo_ip.src_addr = switch_endian32(iface->ip_addr);
    pseudo_ip.zero = 0;
    pseudo_ip.protocol = IP_PROTOCOL_UDP;
    pseudo_ip.udp_length = udp_hdr->length; // already in network order

    udp_hdr->checksum = switch_endian16(compute_udp_checksum(udp_hdr,&pseudo_ip,post_hdr_data,post_hdr_data_len));

    *write_off -= sizeof(udp_header_t);

    return UDP_HDR_RET_SUCESS;
}

void udp_handle_packet(uint8_t* data, uint32_t len, uint32_t src_ip, uint32_t dst_ip){
    if (len < sizeof(udp_header_t)) return;

    udp_header_t* udp_hdr = (udp_header_t*)(data);
    pseudo_ip_hdr_t pseudo;
    pseudo.src_addr = switch_endian32(src_ip); // host -> netword order
    pseudo.zero = 0;
    pseudo.udp_length = udp_hdr->length; // still in network byte order
    pseudo.protocol = IP_PROTOCOL_UDP;
    pseudo.dst_addr = switch_endian32(dst_ip);
    uint8_t* payload = (uint8_t*)udp_hdr + sizeof(udp_header_t);
    uint16_t payload_size = len - sizeof(udp_header_t);

    if (compute_udp_checksum(udp_hdr,&pseudo,payload,payload_size) != 0) 
        return;
    
    uint16_t total_len = switch_endian16(udp_hdr->length);
    if (total_len != len) return; // should be the same

    uint16_t dst_port = switch_endian16(udp_hdr->dst_port);
    uint16_t src_port = switch_endian16(udp_hdr->src_port);

    mutex_wait(&udp_sock_queue_lock,LOCK_TIMEOUT_INF);
    udp_socket_t* sock = find_target_udp_socket(dst_ip,dst_port);
    if (!sock) {
        mutex_signal(&udp_sock_queue_lock);
        return;
    }
    
    mutex_wait(&sock->sock.lock,LOCK_TIMEOUT_INF);
    mutex_signal(&udp_sock_queue_lock);
    udp_enqueue_rx_data(sock,payload,payload_size,src_ip,src_port);
    mutex_signal(&sock->sock.lock);
}

proto_handles_t udp_proto_handles = {
    .bind = udp_bind,
    .recvfrom = udp_recvfrom,
    .sendto = udp_sendto,
};