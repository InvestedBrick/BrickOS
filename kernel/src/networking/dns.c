#include "dns.h"
#include "../processes/process.h"
#include "../tables/syscalls.h"
#include "../io/log.h"
#include "networking.h"
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

static int dns_name_encode(const unsigned char* name, uint8_t* out, size_t cap)
{
    size_t o = 0;
    while (*name) {
        size_t n = 0;
        while (name[n] && name[n] != '.') n++;
        if (n == 0 || n > DNS_MAX_LABEL_LEN) return -1;
        if (o + 1 + n + 1 > cap) return -1;
        out[o++] = (uint8_t)n;
        memcpy(out + o, name, n);
        o += n;
        name += n;
        if (*name == '.') name++;
    }
    if (o + 1 > cap) return -1;
    out[o++] = 0;
    if (o > DNS_MAX_DOMAIN_NAME_LEN) return -1;
    return (int)o;
}

static dns_ret_code_e dns_write_query(uint8_t* data, size_t cap, uint16_t xid,
                                      uint16_t flags, const char* domain_name,
                                      size_t* out_len)
{
    if (!data || !domain_name || !out_len) return DNS_RET_FAILURE;
    if (cap < sizeof(dns_header_t) + sizeof(dns_question_tail_t)) return DNS_RET_FAILURE;

    dns_header_t* hdr = (dns_header_t*)data;
    hdr->xid         = switch_endian16(xid);
    hdr->flags       = switch_endian16(flags);
    hdr->n_questions = switch_endian16(1);
    hdr->n_answers   = 0;
    hdr->n_auth_rrs  = 0;
    hdr->n_add_rrs   = 0;

    size_t name_cap = cap - sizeof(dns_header_t) - sizeof(dns_question_tail_t);
    int n = dns_name_encode(domain_name, data + sizeof(dns_header_t), name_cap);
    if (n < 0) return DNS_RET_FAILURE;

    dns_question_tail_t* tail = (dns_question_tail_t*)(data + sizeof(dns_header_t) + n);
    tail->qtype  = switch_endian16(DNS_TYPE_A);
    tail->qclass = switch_endian16(DNS_CLASS_IN);

    *out_len = sizeof(dns_header_t) + (size_t)n + sizeof(dns_question_tail_t);
    return DNS_RET_SUCCESS;
}

uint32_t dns_lookup(const unsigned char* domain_name){
    if (!domain_name || !dns_socket_fd) return 0;


}
