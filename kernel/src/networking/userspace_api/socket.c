#include "socket.h"
#include "../udp.h"
#include "../ip.h"
#include "../../filesystem/vfs/vfs.h"
#include "../../memory/kmalloc.h"
#include "../../io/log.h"

void init_socket_queues(){
    mutex_init(&udp_sock_queue_lock);
    mutex_init(&raw_ip_sock_lock);
}

socket_t* valid_socket(user_process_t* p, uint32_t fd){
    if (fd > MAX_FDS) return nullptr;

    generic_file_t* file = p->fd_table[fd];
    if (!file || file->type != FILE_TYPE_SOCKET || !file->generic_data) return nullptr;
    
    return (socket_t*)file->generic_data;
}

uint8_t handle_socket_setopt(socket_t* sock, uint32_t optname, void* optval, uint32_t optlen){
    if (!optval) return SOCKET_SETOPTS_FAILURE;

    switch (optname)
    {
    case SO_RCVTIMEOUT:
        if (optlen != sizeof(sock->sock_opts.recv_timeout)) 
            return SOCKET_SETOPTS_FAILURE;
        
        sock->sock_opts.recv_timeout = MS_TO_TICKS(*(uint64_t*)optval);
        break;
    case SO_SNDTIMEOUT:
        if (optlen != sizeof(sock->sock_opts.send_timeout)) 
            return SOCKET_SETOPTS_FAILURE;
        
        sock->sock_opts.send_timeout = MS_TO_TICKS(*(uint64_t*)optval);
        break;
    case SO_REUSEADDR:
        if (optlen != sizeof(sock->sock_opts.reuse_addr))
            return SOCKET_SETOPTS_FAILURE;
    
        sock->sock_opts.reuse_addr = *(uint8_t*)optval;
        break;
    case SO_RCVBUF:
        if (optlen != sizeof(sock->sock_opts.recv_max_buf_size ))
            return SOCKET_SETOPTS_FAILURE;

        sock->sock_opts.recv_max_buf_size = *(uint32_t*)optval;
        break;
    case SO_SNDBUF:
        if (optlen != sizeof(sock->sock_opts.send_max_buf_size)) 
            return SOCKET_SETOPTS_FAILURE;

        sock->sock_opts.send_max_buf_size = *(uint32_t*)optval;
        break;
    default:
        return SOCKET_SETOPTS_FAILURE;
    }

    return SOCKET_SETOPTS_SUCCESS;
}

uint8_t handle_ip_setopt(user_process_t* p,socket_t* sock, uint32_t optname, void* optval, uint32_t optlen){
    if (!optval) return SOCKET_SETOPTS_FAILURE;

    switch (optname)
    {
    case IP_TOS:
        if (optlen != sizeof(sock->ip_opts.tos)) 
            return SOCKET_SETOPTS_FAILURE;
        
        uint8_t new_tos = *(uint8_t*)optval;
        if (new_tos & IP_TOS_PREC_MASK && p->priv_lvl > PRIV_SPECIAL) 
            return SOCKET_SETOPTS_FAILURE; // you are not that important
        
        sock->ip_opts.tos = new_tos & ~0x3; // bottom 2 bits must be 0
        break;
    case IP_TTL:
        if (optlen != sizeof(sock->ip_opts.ttl)) 
            return SOCKET_SETOPTS_FAILURE;
    
        sock->ip_opts.ttl = *(uint8_t*)optval;
        break;
    case IP_HDRINCL:
        if (sock->sock_type != SOCKET_TYPE_RAW) 
            return SOCKET_SETOPTS_FAILURE;

        if (optlen != sizeof(uint8_t)) 
            return SOCKET_SETOPTS_FAILURE;
        
        sock->ip_opts.hdr_incl = *(uint8_t*)optval;
        break;
    default:
        return SOCKET_SETOPTS_FAILURE;
    }

    return SOCKET_SETOPTS_SUCCESS;
}

uint8_t handle_socket_getopt(socket_t* sock, uint32_t optname, void* optval, uint32_t* optlen){
    if (!optval || !optlen) return SOCKET_GETOPTS_FAILURE;
    switch (optname)
    {
    case SO_RCVTIMEOUT:
        if (*optlen < sizeof(sock->sock_opts.recv_timeout)) 
            return SOCKET_GETOPTS_FAILURE;
        
        *(uint64_t*)optval = TICKS_TO_MS(sock->sock_opts.recv_timeout);
        *optlen = sizeof(sock->sock_opts.recv_timeout);
        break;
    case SO_SNDTIMEOUT:
        if (*optlen < sizeof(sock->sock_opts.send_timeout)) 
            return SOCKET_GETOPTS_FAILURE;
        
        *(uint64_t*)optval = TICKS_TO_MS(sock->sock_opts.send_timeout);
        *optlen = sizeof(sock->sock_opts.send_timeout);
        break;
    case SO_REUSEADDR:
        if (*optlen < sizeof(sock->sock_opts.reuse_addr)) 
            return SOCKET_GETOPTS_FAILURE;
        
        *(uint8_t*)optval = sock->sock_opts.reuse_addr;
        *optlen = sizeof(sock->sock_opts.reuse_addr);
        break;
    case SO_RCVBUF:
        if (*optlen < sizeof(sock->sock_opts.recv_max_buf_size )) 
            return SOCKET_GETOPTS_FAILURE;
        
        *(uint32_t*)optval = sock->sock_opts.recv_max_buf_size;
        *optlen = sizeof(sock->sock_opts.recv_max_buf_size);
        break;
    case SO_SNDBUF:
        if (*optlen < sizeof(sock->sock_opts.send_max_buf_size)) 
            return SOCKET_GETOPTS_FAILURE;
        
        *(uint32_t*)optval = sock->sock_opts.send_max_buf_size;
        *optlen = sizeof(sock->sock_opts.send_max_buf_size);
        break;
    default:
        return SOCKET_GETOPTS_FAILURE;
    }

    return SOCKET_GETOPTS_SUCCESS;
}

uint8_t handle_ip_getopt(socket_t* sock, uint32_t optname, void* optval, uint32_t* optlen){
    if (!optval || !optlen) return SOCKET_GETOPTS_FAILURE;

    switch (optname)
    {
    case IP_TOS:
        if (*optlen < sizeof(sock->ip_opts.tos))
            return SOCKET_GETOPTS_FAILURE;

        *(uint8_t*)optval = sock->ip_opts.tos;
        *optlen = sizeof(sock->ip_opts.tos);
        break;
    case IP_TTL:
        if (*optlen < sizeof(sock->ip_opts.ttl))
            return SOCKET_GETOPTS_FAILURE;

        *(uint8_t*)optval = sock->ip_opts.ttl;
        *optlen = sizeof(sock->ip_opts.ttl);
        break;
    case IP_HDRINCL:
        if (*optlen < sizeof(sock->ip_opts.hdr_incl))
            return SOCKET_GETOPTS_FAILURE;

        *(uint8_t*)optval = sock->ip_opts.hdr_incl;
        *optlen = sizeof(sock->ip_opts.hdr_incl);
        break;
    default:
        return SOCKET_GETOPTS_FAILURE;
    }

    return SOCKET_GETOPTS_SUCCESS;
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
        sock->prot_sock = (generic_proto_socket_t*)kmalloc(sizeof(udp_socket_t));
        sock->proto_ops = &udp_proto_handles;
        sock->cleanup_prot_sock = udp_cleanup_sock;
        init_prot_socket(sock->prot_sock,sizeof(udp_socket_t),(generic_proto_socket_t**)&udp_sock_head,&udp_sock_queue_lock);
        break;
    case (SOCKET_INET << 16 | SOCKET_TYPE_STREAM << 8 | 0): 
    case (SOCKET_INET << 16 | SOCKET_TYPE_STREAM << 8 | IP_PROTOCOL_TCP): 
        return SOCKET_OPS_INIT_FAILURE;
    case (SOCKET_INET << 16 | SOCKET_TYPE_RAW << 8 | IP_PROTOCOL_RAW1):
    case (SOCKET_INET << 16 | SOCKET_TYPE_RAW << 8 | IP_PROTOCOL_RAW2):
        sock->prot_sock = (generic_proto_socket_t*)kmalloc(sizeof(raw_ip_socket_t));
        sock->proto_ops = &raw_ip_proto_handles;
        sock->cleanup_prot_sock = raw_ip_cleanup_sock;
        init_prot_socket(sock->prot_sock,sizeof(raw_ip_socket_t),(generic_proto_socket_t**)&raw_ip_sock_head,&raw_ip_sock_lock);
        ((raw_ip_socket_t*)sock->prot_sock)->protocol = protocol;
        break;
    case (SOCKET_INET << 16 | SOCKET_TYPE_RAW | IP_PROTOCOL_ICMP):
    default:
        return SOCKET_OPS_INIT_FAILURE;
    }

    sock->prot_sock->parent_sock = sock;

    sock->ip_opts.ttl = IP_TTL_MAX;
    sock->ip_opts.tos = IP_TOS_DEFAULT;
    sock->ip_opts.hdr_incl = 0;

    sock->sock_opts.reuse_addr = 0;
    sock->sock_opts.recv_timeout  = THREAD_ETERNAL_SLEEP;
    sock->sock_opts.send_timeout  = THREAD_ETERNAL_SLEEP;
    sock->sock_opts.recv_max_buf_size = SOCK_DEFAULT_MAX_BUF_SZ;
    sock->sock_opts.send_max_buf_size = SOCK_DEFAULT_MAX_BUF_SZ;

    return SOCKET_OPS_INIT_SUCCESS;
}

void erase_packet_from_rx_queue(generic_proto_socket_t* sock, recvd_packet_t* packet){
    mutex_wait(&sock->lock,LOCK_TIMEOUT_INF);
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
    sock->rx_queue_size -= packet->data_len;
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
    sock->rx_queue_size = 0;
}
void socket_clear_wait_queue(generic_proto_socket_t* sock){
    while(sock->wait_queue){
        packet_waiting_thread_t* wthread = sock->wait_queue;
        sock->wait_queue = sock->wait_queue->next;
        wakeup_thread(wthread->thread);
        kfree(wthread);
    }
}

void add_packet_waiting_thread(generic_proto_socket_t* sock, thread_t* thread, uint64_t sleep_ticks){
    mutex_wait(&sock->lock,LOCK_TIMEOUT_INF);
    packet_waiting_thread_t* wthread = (packet_waiting_thread_t*)kmalloc(sizeof(packet_waiting_thread_t));
    wthread->thread = thread;
    wthread->next = nullptr;

    if (!sock->wait_queue) sock->wait_queue = wthread;
    else{
        packet_waiting_thread_t* curr = sock->wait_queue;
        while(curr->next) curr = curr->next;
        curr->next = wthread;
    }

    add_sleeping_thread(thread,sleep_ticks);
    mutex_signal(&sock->lock);
}

void enqueue_rx_data(generic_proto_socket_t* sock, recvd_packet_t* packet){

    if (sock->rx_queue_size + packet->data_len > sock->parent_sock->sock_opts.recv_max_buf_size){
        // drop the packet
        kfree(packet->data);
        kfree(packet);
        return;
    }
    sock->rx_queue_size += packet->data_len;

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
    mutex_wait(queue_lock,LOCK_TIMEOUT_INF);
    generic_proto_socket_t* curr = *queue_head;
    if (!curr) *queue_head = sock;
    else{
        while (curr->next) curr = curr->next;

        curr->next = sock;
    }
    mutex_signal(queue_lock);
}

void cleanup_socket(generic_proto_socket_t** queue_head,generic_proto_socket_t* sock,mutex_t* queue_lock){
    mutex_wait(queue_lock,LOCK_TIMEOUT_INF);
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
    mutex_wait(&sock->lock,LOCK_TIMEOUT_INF);
    
    socket_clear_rx_queue(sock);
    socket_clear_wait_queue(sock);

    kfree(sock);
}

void init_prot_socket(generic_proto_socket_t* sock, uint32_t sock_size,generic_proto_socket_t** queue_head,mutex_t* queue_lock){
    memset(sock,0x0,sock_size);
    mutex_init(&sock->lock);


    insert_socket(queue_head,sock,queue_lock);
}