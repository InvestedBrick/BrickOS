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

int udp_bind(socket_t* sock, sockaddr_t* addr, uint32_t len){
    return 0;
}
int udp_sendto(socket_t* sock, void* buf, uint32_t buf_len, uint32_t flags, sockaddr_t* dst_addr, uint32_t addr_len){
    return 0;
}

int udp_recvfrom(socket_t* sock, void* buf, uint32_t buf_len, uint32_t flags, sockaddr_t* src_addr, uint32_t addr_len){
    return 0;
}

void init_sock_queue(){
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

void remove_socket(udp_socket_t* sock){
    mutex_wait(&sock_queue_lock,TIMEOUT_INF);
    if (sock_head == sock) sock_head = sock->next;
    else{
        udp_socket_t* prev = sock_head;
        while(prev->next && prev->next != sock) prev = prev->next;
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

udp_socket_t* init_udp_socket(uint32_t ip_addr, uint16_t port, user_process_t* owner_proc){
    udp_socket_t* sock = (udp_socket_t*)kmalloc(sizeof(udp_socket_t));
    mutex_init(&sock->lock);
    sock->next = nullptr;
    sock->rx_queue = nullptr;
    sock->wait_queue = nullptr;
    sock->ip_addr = ip_addr;
    sock->port = port;
    sock->owner_proc = owner_proc;

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

    *write_off -= sizeof(udp_header_t);

    return UDP_HDR_RET_SUCESS;
}

proto_handles_t sock_handles = {
    .bind = udp_bind,
    .recvfrom = udp_recvfrom,
    .sendto = udp_sendto,
};