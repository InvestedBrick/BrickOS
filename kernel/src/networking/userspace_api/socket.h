#ifndef INCLUDE_SOCKET_H
#define INCLUDE_SOCKET_H

#include "../network_defines.h"
#include "../../filesystem/vfs/vfs.h"
#include "../../processes/spinlocks.h"
#include "../../processes/scheduler.h"

#define SOCK_DEFAULT_MAX_BUF_SZ (64 * 1024) 
typedef struct socket socket_t; 
typedef enum {
    SOCKET_UNCONNECTED,
    SOCKET_CONNECTED
}socket_state_e;

typedef struct packet_waiting_thread{
    thread_t* thread;
    struct packet_waiting_thread* next;
}packet_waiting_thread_t;

typedef struct recvd_packet {
    uint8_t* data;
    uint32_t data_len;
    struct recvd_packet* next;
}recvd_packet_t;

typedef struct generic_proto_socket {
    packet_waiting_thread_t* wait_queue;
    recvd_packet_t* rx_queue;
    mutex_t lock;
    struct generic_proto_socket* next;
}generic_proto_socket_t;

typedef struct {
    int (*bind)     (socket_t* sock, sockaddr_t* addr, uint32_t len);
    int (*sendto)   (socket_t* sock, void* buf, uint32_t buf_len, uint32_t flags, sockaddr_t* dst_addr, uint32_t addr_len);
    int (*recvfrom) (socket_t* sock, void* buf, uint32_t buf_len, uint32_t flags, sockaddr_t* src_addr, uint32_t addr_len);
}proto_handles_t;

typedef struct {
    uint64_t recv_timeout;
    uint64_t send_timeout;
    uint32_t recv_buf_size;
    uint32_t send_buf_size;
    uint8_t reuse_addr; // > 0 means do reuse
}socket_opts_t;

typedef struct {
    uint8_t ttl;
    uint8_t tos;
    uint8_t hdr_incl; // > 0 means user includes header 
}ip_opts_t;

typedef struct socket {
    socket_state_e sock_state;
    socket_type_e sock_type;
    proto_handles_t* proto_ops;
    
    socket_opts_t sock_opts;
    ip_opts_t ip_opts;

    generic_proto_socket_t* prot_sock; // protocol specific socket for managing queues etc
    void (*cleanup_prot_sock)(generic_proto_socket_t*);    
}socket_t;
#define SOCKET_OPS_INIT_SUCCESS 0x0
#define SOCKET_OPS_INIT_FAILURE 0x1

#define SOCKET_PROT_SOCK_SUCCESS 0x0
#define SOCKET_PROT_SOCK_FAILURE 0x1

#define SOCKET_SETOPTS_SUCCESS 0x0
#define SOCKET_SETOPTS_FAILURE 0x1
extern vfs_handles_t socket_handles;

/**
 * valid_socket:
 * Validates if a socket is present at the fd table under the given fd and whether the underlying socket struct is valid
 * @param p The process
 * @param fd The file descriptor
 * @return The socket at fd_table[fd]->generic_data if success, nullptr otherwise
 */
socket_t* valid_socket(user_process_t* p, uint32_t fd);

/**
 * handle_socket_setopt:
 * Handles socket level setsockopt calls
 * @param sock The socket
 * @param optname The option name
 * @param optval The option value
 * @param optlen The length of the option value
 * @return SOCKET_SETOPTS_SUCCESS upon success, SOCKET_SETOPTS_FAILURE otherwise
 */
uint8_t handle_socket_setopt(socket_t* sock, uint32_t optname, void* optval, uint32_t optlen);

/**
 * handle_ip_setopt:
 * Handles IP level setsockopt calls
 * @param p The user process (needed for TOS priviledge check)
 * @param sock The socket
 * @param optname The option name
 * @param optval The option value
 * @param optlen The length of the option value
 * @return SOCKET_SETOPTS_SUCCESS upon success, SOCKET_SETOPTS_FAILURE otherwise
 */
uint8_t handle_ip_setopt(user_process_t* p,socket_t* sock, uint32_t optname, void* optval, uint32_t optlen);

/**
 * init_socket_ops:
 * Assigns a socket the relevant operations to be called upon their respective syscall
 * @param sock The generic socket
 * @param domain The domain in which the socket operates
 * @param type The socket type
 * @param protocol The protocol
 * @return SOCKET_OPS_INIT_SUCCESS upon success, SOCKET_OPS_INIT_FAILURE otherwise 
 */
uint8_t init_socket(socket_t* sock, socket_domain_e domain, socket_type_e type, uint8_t protocol);


/**
 * init_socket_queues:
 * Initializes the socket queues for all protocols
 */
void init_socket_queues();

/**
 * erase_packet_from_rx_queue:
 * Erases a packet from the socket's receive queue and frees its memory
 * @param sock The socket
 * @param packet The packet to erase
 */
void erase_packet_from_rx_queue(generic_proto_socket_t* sock, recvd_packet_t* packet);

/**
 * socket_clear_rx_queue:
 * Clears the receive queue of a socket and frees all memory
 * @param sock The socket
 */
void socket_clear_rx_queue(generic_proto_socket_t* sock);

/**
 * socket_clear_wait_queue:
 * Clears the wait queue of a socket and wakes up all waiting threads
 */
void socket_clear_wait_queue(generic_proto_socket_t* sock);

/**
 * add_packet_waiting_thread:
 * Adds a thread to the socket's wait queue and puts it to sleep 
 * @param sock The socket
 * @param thread The thread to add
 * @param sleep_ticks The timeout to sleep for (THREAD_ETERNAL_SLEEP to wait until awoken manually)
 */
void add_packet_waiting_thread(generic_proto_socket_t* sock, thread_t* thread, uint64_t sleep_ticks);

/**
 * enqueue_rx_data:
 * Enqueues a received packet to the socket's receive queue and wakes up a waiting thread if any are waiting
 * @param sock The socket
 * @param packet The received packet to enqueue
 */
void enqueue_rx_data(generic_proto_socket_t* sock, recvd_packet_t* packet);

/**
 * insert_socket:
 * Inserts a socket into the protocol's socket queue
 * @param queue_head The head of the protocol's socket queue
 * @param sock The socket to insert
 * @param queue_lock The lock for the protocol's socket queue
 */
void insert_socket(generic_proto_socket_t** queue_head,generic_proto_socket_t* sock,mutex_t* queue_lock);

/**
 * cleanup_socket:
 * Cleans up a socket by removing it from the protocol's socket queue, clearing its receive and wait queues, and freeing its memory
 * @param queue_head The head of the protocol's socket queue
 * @param sock The socket to clean up
 * @param queue_lock The lock for the protocol's socket queue
 */
void cleanup_socket(generic_proto_socket_t** queue_head,generic_proto_socket_t* sock,mutex_t* queue_lock);

/**
 * init_prot_socket:
 * Initializes a protocol-specific socket by zeroing its memory, initializing its lock, and inserting it into the protocol's socket queue
 * @param sock The protocol-specific socket to initialize
 * @param sock_size The size of the protocol-specific socket structure
 * @param queue_head The head of the protocol's socket queue
 */
void init_prot_socket(generic_proto_socket_t* sock, uint32_t sock_size,generic_proto_socket_t** queue_head,mutex_t* queue_lock);
#endif