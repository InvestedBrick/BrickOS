#include "socket.h"
#include "../udp.h"
#include "../../filesystem/vfs/vfs.h"
#include "../../memory/kmalloc.h"
#include "../../io/log.h"

void init_socket_queues(){
    mutex_init(&udp_sock_queue_lock);
}

int socket_close(generic_file_t* file){
    socket_t* sock = (socket_t*)file->generic_data;

    if (sock->prot_sock && sock->cleanup_prot_sock){
        sock->cleanup_prot_sock(sock->prot_sock);
    }
    
    return 0;
} 

vfs_handles_t socket_handles = {
    .close = socket_close,
    .read = 0,
    .write = 0,
    .ioctl = 0,
    .seek = 0,
};

uint8_t init_socket(socket_t* sock, socket_domain_e domain, socket_type_e type, uint8_t protocol){
    uint32_t merge = ((uint32_t)domain << 16) | ((uint32_t)type << 8) | (uint32_t)protocol;
    switch (merge)
    {
    case (SOCKET_INET << 16 | SOCKET_TYPE_DGRAM << 8 | 0 ): 
    case (SOCKET_INET << 16 | SOCKET_TYPE_DGRAM << 8 | IP_PROTOCOL_UDP ): 
        log("HERE");
        sock->prot_sock = (generic_proto_socket_t*)kmalloc(sizeof(udp_socket_t));
        sock->proto_ops = &udp_proto_handles;
        sock->cleanup_prot_sock = udp_cleanup_sock;
        init_prot_socket(sock->prot_sock,sizeof(udp_socket_t),(generic_proto_socket_t*)&udp_sock_head,&udp_sock_queue_lock);
        break;
    case (SOCKET_INET << 16 | SOCKET_TYPE_STREAM << 8 | 0): 
    case (SOCKET_INET << 16 | SOCKET_TYPE_STREAM << 8 | IP_PROTOCOL_TCP): 
    case (SOCKET_INET << 16 | SOCKET_TYPE_RAW | IP_PROTOCOL_RAW1):
    case (SOCKET_INET << 16 | SOCKET_TYPE_RAW | IP_PROTOCOL_RAW2):
    case (SOCKET_INET << 16 | SOCKET_TYPE_RAW | IP_PROTOCOL_ICMP):
    default:
        return SOCKET_OPS_INIT_FAILURE;
    }

    return SOCKET_OPS_INIT_SUCCESS;
}

void erase_packet_from_rx_queue(generic_proto_socket_t* sock, recvd_packet_t* packet){
    mutex_wait(&sock->lock,TIMEOUT_INF);
    recvd_packet_t* curr = sock->rx_queue;
    if (curr == packet){
        sock->rx_queue = curr->next;

        goto cleanup;
    }
    while(curr && curr->next != packet) curr = curr->next;
    if (!curr) {
        mutex_signal(&sock->lock);
        return;
    }
    curr->next = packet->next;
cleanup:
    kfree(packet->data);
    kfree(packet);
    mutex_signal(&sock->lock);
}

void socket_clear_rx_queue(generic_proto_socket_t* sock){
    while(sock->rx_queue){
        recvd_packet_t* packet = sock->rx_queue;
        sock->rx_queue = sock->rx_queue->next;
        kfree(packet->data);
        kfree(packet);

    }
}
void socket_clear_wait_queue(generic_proto_socket_t* sock){
    while(sock->wait_queue){
        packet_waiting_thread_t* wthread = sock->wait_queue;
        sock->wait_queue = sock->wait_queue->next;
        wakeup_thread(wthread->thread);
        kfree(wthread);
    }
}

void add_packet_waiting_thread(generic_proto_socket_t* sock, thread_t* thread){
    mutex_wait(&sock->lock,TIMEOUT_INF);
    packet_waiting_thread_t* wthread = (packet_waiting_thread_t*)kmalloc(sizeof(packet_waiting_thread_t));
    wthread->thread = thread;
    wthread->next = nullptr;

    if (!sock->wait_queue) sock->wait_queue = wthread;
    else{
        packet_waiting_thread_t* curr = sock->wait_queue;
        while(curr->next) curr = curr->next;
        curr->next = wthread;
    }

    add_sleeping_thread(thread,THREAD_ETERNAL_SLEEP);
    mutex_signal(&sock->lock);
}

void enqueue_rx_data(generic_proto_socket_t* sock, recvd_packet_t* packet){
    recvd_packet_t* curr = sock->rx_queue;
    if (!curr) sock->rx_queue = packet;
    else{
        while(curr->next) curr = curr->next;
        curr->next = packet;
    }

    packet_waiting_thread_t* wthread = sock->wait_queue;
    if (wthread){
        sock->wait_queue = sock->wait_queue->next;
        wakeup_thread(wthread->thread);
        kfree(wthread);
    }
}

void insert_socket(generic_proto_socket_t** queue_head,generic_proto_socket_t* sock,mutex_t* queue_lock){
    mutex_wait(queue_lock,TIMEOUT_INF);
    generic_proto_socket_t* curr = *queue_head;
    if (!curr) *queue_head = sock;
    else{
        while (curr->next) curr = curr->next;

        curr->next = sock;
    }
    mutex_signal(queue_lock);
}

void cleanup_socket(generic_proto_socket_t** queue_head,generic_proto_socket_t* sock,mutex_t* queue_lock){
    mutex_wait(queue_lock,TIMEOUT_INF);
    if (*queue_head == sock) *queue_head = sock->next;
    else{
        generic_proto_socket_t* prev = *queue_head;
        while(prev->next && prev->next != sock) prev = prev->next;
        if (!prev->next){
            mutex_signal(queue_lock);
            return;
        }
        prev->next = sock->next;
    }
    mutex_signal(queue_lock);
    mutex_wait(&sock->lock,TIMEOUT_INF);
    
    socket_clear_rx_queue(sock);
    socket_clear_wait_queue(sock);

    kfree(sock);
}

void init_prot_socket(generic_proto_socket_t* sock, uint32_t sock_size,generic_proto_socket_t** queue_head,mutex_t* queue_lock){
    memset(sock,0x0,sock_size);
    mutex_init(&sock->lock);


    insert_socket(queue_head,sock,queue_lock);
}