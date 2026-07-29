#include "udp.h"
#include "userspace_api/socket.h"
#include "networking.h"
#include "ip.h"

int udp_bind(socket_t* sock, sockaddr_t* addr, uint32_t len){

}
int udp_sendto(socket_t* sock, void* buf, uint32_t buf_len, uint32_t flags, sockaddr_t* dst_addr, uint32_t addr_len){

}

int udp_recvfrom(socket_t* sock, void* buf, uint32_t buf_len, uint32_t flags, sockaddr_t* src_addr, uint32_t addr_len){
    
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