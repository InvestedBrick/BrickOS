#ifndef INCLUDE_SOCKET_H
#define INCLUDE_SOCKET_H

#include "../network_defines.h"
#include "../../filesystem/vfs/vfs.h"
typedef struct socket socket_t; 
typedef enum {
    SOCKET_UNCONNECTED,
    SOCKET_CONNECTED
}socket_state_e;

typedef struct {
    int (*bind)     (socket_t* sock, sockaddr_t* addr, uint32_t len);
    int (*sendto)   (socket_t* sock, void* buf, uint32_t buf_len, uint32_t flags, sockaddr_t* dst_addr, uint32_t addr_len);
    int (*recvfrom) (socket_t* sock, void* buf, uint32_t buf_len, uint32_t flags, sockaddr_t* src_addr, uint32_t addr_len);
}proto_handles_t;

typedef struct socket{
    socket_state_e sock_state;
    socket_type_e sock_type;
    proto_handles_t* proto_ops;
    void* prot_sock; // protocol specific socket for managing queues etc
}socket_t;

#define SOCKET_OPS_INIT_SUCCESS 0x0
#define SOCKET_OPS_INIT_FAILURE 0x1

#define SOCKET_PROT_SOCK_SUCCESS 0x0
#define SOCKET_PROT_SOCK_FAILURE 0x1

extern vfs_handles_t socket_handles;

/**
 * init_socket_ops:
 * Assigns a socket the relevant operations to be called upon their respective syscall
 * @param sock The generic socket
 * @param domain The domain in which the socket operates
 * @param type The socket type
 * @return SOCKET_OPS_INIT_SUCCESS upon success, SOCKET_OPS_INIT_FAILURE otherwise 
 */
uint8_t init_socket_ops(socket_t* sock, socket_domain_e domain, socket_type_e type);


/**
 * init_protocol_specific_socket:
 * Initializes the protocol specific socket for a generic socket
 * @param sock The generic socket
 * @param domain The domain in which the socket operates
 * @param type The socket type
 * @return SOCKET_PROT_SOCK_SUCCESS upon success, SOCKET_PROT_SOCK_FAILURE otherwise
 */
uint8_t init_protocol_specific_socket(socket_t* sock, socket_domain_e domain, socket_type_e type);
#endif