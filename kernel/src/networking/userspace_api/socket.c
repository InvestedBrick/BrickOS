#include "socket.h"
#include "../udp.h"
#include "../../filesystem/vfs/vfs.h"

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

uint8_t init_socket_ops(socket_t* sock, socket_domain_e domain, socket_type_e type){
    uint32_t merge = ((uint32_t)domain << 16) | (uint32_t)type;
    switch (merge)
    {
    case (SOCKET_INET << 16 | SOCKET_TYPE_DGRAM):
        // UDP
        sock->proto_ops = &udp_proto_handles;
        sock->cleanup_prot_sock = cleanup_udp_socket;
        break;
    case (SOCKET_INET << 16 | SOCKET_TYPE_STREAM): // TCP
    default:
        return SOCKET_OPS_INIT_FAILURE;
    }

    return SOCKET_OPS_INIT_SUCCESS;
}

uint8_t init_protocol_specific_socket(socket_t* sock, socket_domain_e domain, socket_type_e type){
    
    uint32_t merge = ((uint32_t)domain << 16) | (uint32_t)type;
    switch (merge)
    {
    case (SOCKET_INET << 16 | SOCKET_TYPE_DGRAM):
        // UDP
        sock->prot_sock = init_udp_socket();
        break;
    case (SOCKET_INET << 16 | SOCKET_TYPE_STREAM): // TCP
    default:
        return SOCKET_PROT_SOCK_FAILURE;
    }

    return SOCKET_PROT_SOCK_SUCCESS;
}