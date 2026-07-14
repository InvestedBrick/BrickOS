#include "ip.h"
#include "networking.h"


uint8_t ip_add_header(uint8_t* data, uint32_t* write_off,uint32_t dst_addr,uint8_t prot, uint8_t tos, uint8_t ttl,uint16_t id,uint8_t df,uint32_t post_hdr_data_len){
    if (*write_off < sizeof (ipv4_header_t)) return IP_HDR_RET_DATA_OVERFLOW;
    ipv4_header_t* hdr = (ipv4_header_t*)(data + *write_off - sizeof(ipv4_header_t));

    hdr->version_ihl = (IP_VERSION_4 << 4) | (IP_HDR_DEFAULT_SIZE / sizeof(uint32_t));
    hdr->type_of_service = tos;

    hdr->ident = switch_endian16(id);
    hdr->protocol = prot;
    hdr->time_to_live = ttl;
    hdr->flags_fragment_offset = 0;
    if (df) hdr->flags_fragment_offset |= switch_endian16(IP_FLAGS_DONT_FRAGMENT);
    hdr->src_ip = switch_endian32(nic_driver->ip_addr);
    hdr->dst_ip = switch_endian32(dst_addr);
    hdr->total_length = switch_endian16(IP_HDR_DEFAULT_SIZE + post_hdr_data_len);
    hdr->header_checksum = 0;
    hdr->header_checksum = switch_endian16(compute_checksum((uint8_t*)hdr,IP_HDR_DEFAULT_SIZE));

    *write_off -= sizeof(ipv4_header_t);
    return IP_HDR_RET_SUCCESS;
}

void ip_handle_packet(uint8_t* data, uint32_t write_off, uint32_t total_len) {

}