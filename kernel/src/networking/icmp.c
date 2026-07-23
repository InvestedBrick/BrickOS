#include "icmp.h"
#include "networking.h"
#include "../utilities/util.h"
#include "ip.h"
#include "ethernet.h"
#include "../memory/kmalloc.h"
#include <stdatomic.h>

static atomic_uint_fast16_t icmp_id;

uint16_t get_new_icmp_ident(){
    return atomic_fetch_add(&icmp_id,1);
}


void icmp_add_extra_payload(uint8_t* data, uint32_t* write_off, uint8_t* payload, uint32_t payload_len){
    *write_off -= payload_len;
    memcpy(data + *write_off, payload, payload_len);
}

uint8_t icmp_add_hdr(uint8_t* data, uint32_t* write_off, uint8_t icmp_type, uint8_t icmp_code, uint32_t may_be_used_dword, uint8_t* extra_payload, uint32_t extra_payload_len){
    if (*write_off < sizeof(icmp_header_t) + extra_payload_len) return ICMP_HDR_RET_DATA_OVERFLOW;

    if (extra_payload && extra_payload_len){
        icmp_add_extra_payload(data, write_off, extra_payload, extra_payload_len);
    }
    *write_off -= sizeof(icmp_header_t);

    icmp_header_t* hdr = (icmp_header_t*)(data + *write_off);
    hdr->icmp_type = icmp_type;
    hdr->icmp_code = icmp_code;
    hdr->checksum  = 0;
    memset(hdr->may_be_used,0x0,sizeof(hdr->may_be_used));
    switch (icmp_type){
        case ICMP_TYPE_ECHO_REPLY:
            ((icmp_echo_t*)(hdr->may_be_used))->echo_ident = switch_endian16((uint16_t)(may_be_used_dword >> 16));
            ((icmp_echo_t*)(hdr->may_be_used))->echo_seq = switch_endian16((uint16_t)(may_be_used_dword & 0xffff));
            hdr->icmp_code = 0;
            break;
        case ICMP_TYPE_DEST_UNR_MSG:
            ((uint32_t*)hdr->may_be_used)[0] = switch_endian32(may_be_used_dword);
            break;
        case ICMP_TYPE_SRC_QUENCH_MSG:
            hdr->icmp_code = 0;
            ((uint32_t*)hdr->may_be_used)[0] = 0;
            break;
        case ICMP_TYPE_REDIR_MSG:
            ((icmp_redir_t*)hdr->may_be_used)->gateway_ip_addr = switch_endian32(may_be_used_dword);
            break;
        case ICMP_TYPE_ECHO_MSG:
            ((icmp_echo_t*)(hdr->may_be_used))->echo_ident = switch_endian16((uint16_t)(may_be_used_dword >> 16));
            ((icmp_echo_t*)(hdr->may_be_used))->echo_seq = switch_endian16((uint16_t)(may_be_used_dword & 0xffff));
            hdr->icmp_code = 0;
            break;
        case ICMP_TYPE_TIME_EXC_MSG:
            ((uint32_t*)hdr->may_be_used)[0] = switch_endian32(may_be_used_dword);
            break;
        case ICMP_TYPE_PARAM_PRBLM_MSG:
            ((icmp_param_problem_t*)hdr->may_be_used)->ptr = (uint8_t)(may_be_used_dword & 0xff);
            break;
        case ICMP_TYPE_TIMESTMP_MSG:
            ((icmp_timestamp_t*)hdr->may_be_used)->time_stmp_ident = switch_endian16((uint16_t)(may_be_used_dword >> 16));
            ((icmp_timestamp_t*)hdr->may_be_used)->time_stmp_seq = switch_endian16((uint16_t)(may_be_used_dword & 0xffff));
            hdr->icmp_code = 0;
            break;
        case ICMP_TYPE_TIMESTMP_REPLY:
            ((icmp_timestamp_t*)hdr->may_be_used)->time_stmp_ident = switch_endian16((uint16_t)(may_be_used_dword >> 16));
            ((icmp_timestamp_t*)hdr->may_be_used)->time_stmp_seq = switch_endian16((uint16_t)(may_be_used_dword & 0xffff));
            hdr->icmp_code = 0;
            break;
        case ICMP_TYPE_INFO_REQ_MSG:
            ((icmp_info_t*)(hdr->may_be_used))->info_ident = switch_endian16((uint16_t)(may_be_used_dword >> 16));
            ((icmp_info_t*)(hdr->may_be_used))->info_seq = switch_endian16((uint16_t)(may_be_used_dword & 0xffff));
            hdr->icmp_code = 0;
            break;
        case ICMP_TYPE_INFO_REPLY_MSG:
            ((icmp_info_t*)(hdr->may_be_used))->info_ident = switch_endian16((uint16_t)(may_be_used_dword >> 16));
            ((icmp_info_t*)(hdr->may_be_used))->info_seq = switch_endian16((uint16_t)(may_be_used_dword & 0xffff));
            hdr->icmp_code = 0;
            break;
        default:
            return ICMP_HDR_RET_INVALID_TYPE;
            break;
    }

    hdr->checksum = switch_endian16(compute_checksum((uint8_t*)hdr, sizeof(icmp_header_t) + extra_payload_len));


    return ICMP_HDR_RET_SUCCESS;
}

void icmp_handle_packet(uint8_t* data, uint32_t len, uint32_t src_ip){
    if (len < sizeof(icmp_header_t)) return;

    if (compute_checksum(data,len) != 0) return;

    icmp_header_t* icmp_hdr = (icmp_header_t*)data;
    switch (icmp_hdr->icmp_type)
    {
    case ICMP_TYPE_ECHO_REPLY:
        icmp_handle_echo_reply(icmp_hdr, len);
        break;
    case ICMP_TYPE_DEST_UNR_MSG:
        icmp_handle_dest_unreachable(icmp_hdr, len);
        break;
    case ICMP_TYPE_SRC_QUENCH_MSG:
        icmp_handle_source_quench(icmp_hdr, len);
        break;
    case ICMP_TYPE_REDIR_MSG:
        icmp_handle_redirect(icmp_hdr, len);
        break;
    case ICMP_TYPE_ECHO_MSG:
        icmp_handle_echo_request(icmp_hdr, len,src_ip);
        break;
    case ICMP_TYPE_TIME_EXC_MSG:
        icmp_handle_time_exceeded(icmp_hdr, len);
        break;
    case ICMP_TYPE_PARAM_PRBLM_MSG:
        icmp_handle_parameter_problem(icmp_hdr, len);
        break;
    case ICMP_TYPE_TIMESTMP_MSG:
        icmp_handle_timestamp(icmp_hdr, len);
        break;
    case ICMP_TYPE_TIMESTMP_REPLY:
        icmp_handle_timestamp_reply(icmp_hdr, len);
        break;
    case ICMP_TYPE_INFO_REQ_MSG:
        icmp_handle_info_request(icmp_hdr, len);
        break;
    case ICMP_TYPE_INFO_REPLY_MSG:
        icmp_handle_info_reply(icmp_hdr, len);
        break;
    default:
        break;
    }

}

void icmp_handle_echo_reply(icmp_header_t* icmp_hdr, uint32_t total_len){
    // WIP
}

void icmp_handle_dest_unreachable(icmp_header_t* icmp_hdr, uint32_t total_len){
    // WIP
}

void icmp_handle_source_quench(icmp_header_t* icmp_hdr, uint32_t total_len){
    // WIP
}

void icmp_handle_redirect(icmp_header_t* icmp_hdr, uint32_t total_len){
    // WIP
}
void icmp_handle_echo_request(icmp_header_t* icmp_hdr, uint32_t total_len, uint32_t src_ip){
    icmp_echo_t* echo = (icmp_echo_t*)(icmp_hdr->may_be_used);
    uint16_t ident = switch_endian16(echo->echo_ident);
    uint16_t seq = switch_endian16(echo->echo_seq);

    icmp_send_data_t* send_data = kmalloc(sizeof(icmp_send_data_t));
    send_data->icmp_code = 0;
    send_data->icmp_type = ICMP_TYPE_ECHO_REPLY;
    send_data->may_be_used = ((uint32_t)ident << 16) | seq;
    send_data->extra_payload = (uint8_t*)icmp_hdr + sizeof(icmp_header_t);
    send_data->extra_payload_len = total_len - sizeof(icmp_header_t);

    send_ip_based_packet(nullptr,
                         0,
                         src_ip,
                         IP_PROTOCOL_ICMP,
                         send_data);

    kfree(send_data);
}

void icmp_handle_time_exceeded(icmp_header_t* icmp_hdr, uint32_t total_len){
    // WIP
}

void icmp_handle_parameter_problem(icmp_header_t* icmp_hdr, uint32_t total_len){
    // WIP
}

void icmp_handle_timestamp(icmp_header_t* icmp_hdr, uint32_t total_len){
    // WIP
}

void icmp_handle_timestamp_reply(icmp_header_t* icmp_hdr, uint32_t total_len){
    // WIP
}

void icmp_handle_info_request(icmp_header_t* icmp_hdr, uint32_t total_len){
    // WIP
}

void icmp_handle_info_reply(icmp_header_t* icmp_hdr, uint32_t total_len){
    // WIP
}
