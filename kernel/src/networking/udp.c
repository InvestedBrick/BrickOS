#include "udp.h"
#include "userspace_api/socket.h"
#include "networking.h"
#include "ip.h"
#include "../memory/kmalloc.h"
#include "../filesystem/vfs/vfs.h"

udp_socket_t* sock_head = nullptr;
mutex_t sock_queue_lock;

/**
 * POLICY:
 * sock_queue_lock must only be signaled once the lock for a found socket has been aquired
 * This is to ensure that a deleted lock is not tried to be accessed
 */

uint8_t is_udp_port_used(uint16_t port){
    mutex_wait(&sock_queue_lock,TIMEOUT_INF);
    udp_socket_t* curr = sock_head;
    while(curr){
        if (curr->port == port) {
            mutex_signal(&sock_queue_lock);
            return 1;
        }
        curr = curr->next;
    }
    mutex_signal(&sock_queue_lock);
    return 0;

}

int udp_bind(socket_t* sock, sockaddr_t* addr, uint32_t len){
    udp_socket_t* udp_sock = (udp_socket_t*)sock->prot_sock;

    if (len != sizeof(in_sockaddr_t)) return UDP_RET_FAIL;
    in_sockaddr_t* inet_sockaddr = (in_sockaddr_t*)addr;
    if (inet_sockaddr->inet_family != INET_FAM_IPv4) return UDP_RET_FAIL;
    
    mutex_wait(&udp_sock->lock,TIMEOUT_INF);
    
    if (is_udp_port_used(inet_sockaddr->inet_port)) {
        mutex_signal(&udp_sock->lock);
        return UDP_RET_FAIL;
    }

    udp_sock->ip_addr = inet_sockaddr->inet_addr;
    udp_sock->port = inet_sockaddr->inet_port;

    mutex_signal(&udp_sock->lock);
    return 0;
}

int udp_sendto(socket_t* sock, void* buf, uint32_t buf_len, uint32_t flags, sockaddr_t* dst_addr, uint32_t addr_len){
    return 0;
}

int udp_recvfrom(socket_t* sock, void* buf, uint32_t buf_len, uint32_t flags, sockaddr_t* src_addr, uint32_t addr_len){
    return 0;
}

void init_udp_sock_queue(){
    mutex_init(&sock_queue_lock);
}

void insert_socket(udp_socket_t* sock){
    mutex_wait(&sock_queue_lock,TIMEOUT_INF);
    udp_socket_t* curr = sock_head;
    if (!curr) sock_head = sock;
    else{
        while (curr->next) curr = curr->next;

        curr->next = sock;
    }
    mutex_signal(&sock_queue_lock);
}

void cleanup_udp_socket(void* sock_ptr){
    udp_socket_t* sock = (udp_socket_t*)sock_ptr; 
    mutex_wait(&sock_queue_lock,TIMEOUT_INF);
    if (sock_head == sock) sock_head = sock->next;
    else{
        udp_socket_t* prev = sock_head;
        while(prev->next && prev->next != sock) prev = prev->next;
        if (!prev->next){
            mutex_signal(&sock_queue_lock);
            return;
        }
        prev->next = sock->next;
    }
    mutex_signal(&sock_queue_lock);
    mutex_wait(&sock->lock,TIMEOUT_INF);
    
    while(sock->wait_queue){
        udp_waiting_thread_t* wthread = sock->wait_queue;
        sock->wait_queue = sock->wait_queue->next;
        wakeup_thread(wthread->thread);
        kfree(wthread);
    }
    
    while(sock->rx_queue){
        udp_recvd_packet_t* packet = sock->rx_queue;
        sock->rx_queue = sock->rx_queue->next;
        kfree(packet->data);
        kfree(packet);

    }

    kfree(sock);

}

udp_socket_t* find_target_udp_socket(uint32_t ip_addr, uint16_t port){
    // must be mutex locked from outside
    udp_socket_t* curr = sock_head;
    while(curr){
        if (curr->ip_addr == ip_addr && curr->port == port) return curr;
        curr = curr->next;
    }
    
    return nullptr;
}

void udp_enqueue_rx_data(udp_socket_t* sock, uint8_t* data, uint16_t data_len){
    // sock->lock must be locked
    udp_recvd_packet_t* packet = (udp_recvd_packet_t*)kmalloc(sizeof(udp_recvd_packet_t));
    uint8_t* data_buffer = (uint8_t*)kmalloc(data_len);
    memcpy(data_buffer,data,data_len);
    packet->next = nullptr;
    packet->data = data_buffer;
    packet->data_len = data_len;


    // enqueue packet
    udp_recvd_packet_t* curr = sock->rx_queue;
    if (!curr) sock->rx_queue = packet;
    else{
        while(curr->next) curr = curr->next;
        curr->next = packet;
    }

    // wakeup a waiting thread
    udp_waiting_thread_t* wthread = sock->wait_queue;
    if (wthread){
        sock->wait_queue = sock->wait_queue->next;
        wakeup_thread(wthread->thread);
        kfree(wthread);
    }

    

}

udp_socket_t* init_udp_socket(){
    udp_socket_t* sock = (udp_socket_t*)kmalloc(sizeof(udp_socket_t));
    mutex_init(&sock->lock);
    sock->next = nullptr;
    sock->rx_queue = nullptr;
    sock->wait_queue = nullptr;
    sock->ip_addr = INADDR_ANY;
    sock->port = 0;
    sock->owner_proc = get_current_user_process();

    insert_socket(sock);

    return sock;
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

    udp_hdr->checksum = compute_udp_checksum(udp_hdr,&pseudo_ip,post_hdr_data,post_hdr_data_len);
    if (!udp_hdr->checksum) udp_hdr->checksum = 0xffff;

    *write_off -= sizeof(udp_header_t);

    return UDP_HDR_RET_SUCESS;
}

void udp_handle_packet(uint8_t* data, uint32_t len, uint32_t src_ip, uint32_t dst_ip){
    if (len < sizeof(udp_header_t)) return;

    udp_header_t* udp_hdr = (udp_header_t*)(data);
    pseudo_ip_hdr_t pseudo;
    pseudo.src_addr = switch_endian32(src_ip); 
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

    mutex_wait(&sock_queue_lock,TIMEOUT_INF);
    udp_socket_t* sock = find_target_udp_socket(dst_ip,dst_port);
    if (!sock) {
        mutex_signal(&sock_queue_lock);
        return;
    }
    
    mutex_wait(&sock->lock,TIMEOUT_INF);
    mutex_signal(&sock_queue_lock);
    udp_enqueue_rx_data(sock,payload,payload_size);
    mutex_signal(&sock->lock);
}

proto_handles_t udp_proto_handles = {
    .bind = udp_bind,
    .recvfrom = udp_recvfrom,
    .sendto = udp_sendto,
};