#include "socket.h"
#include "../udp.h"
#include "../../filesystem/vfs/vfs.h"

int socket_close(generic_file_t* file){
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


    return SOCKET_OPS_INIT_SUCCESS;
}

uint8_t init_protocol_specific_socket(socket_t* sock, socket_domain_e domain, socket_type_e type){
    

    return SOCKET_PROT_SOCK_SUCCESS;
}