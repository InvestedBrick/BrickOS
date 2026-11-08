#include "dns.h"
#include "../processes/process.h"
#include "../tables/syscalls.h"
#include "../io/log.h"
static uint64_t dns_socket_fd;

void dns_init(){
    process_t* curr_proc = get_current_process();
    if (!curr_proc || curr_proc->process_id != global_kernel_process.process_id){
        error("Invalid process tried to init dns");
        return;
    }

    dns_socket_fd = sys_socket(curr_proc, SOCKET_INET, SOCKET_TYPE_DGRAM, IP_PROTOCOL_UDP);
    
    if (dns_socket_fd == SYSCALL_FAIL){
        error("Failed to open dns kernel socket");
        return;
    }

    in_sockaddr_t addr;
    addr.inet_addr = INADDR_ANY;
    addr.inet_family = INET_FAM_IPv4;
    addr.inet_port = DNS_UDP_PORT;

    if (sys_bind(curr_proc,(sockaddr_t*)dns_socket_fd,&addr,sizeof(addr)) == (uint64_t)SYSCALL_FAIL) {
        error("Failed to bind dns kernel socket");
        sys_close(curr_proc,dns_socket_fd);
        return;
    }

}

uint32_t dns_lookup(const unsigned char* domain_name){

}
