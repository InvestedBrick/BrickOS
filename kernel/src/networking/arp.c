#include "arp.h"
#include "../io/log.h"
#include "networking.h"
#include "../memory/kmalloc.h"
#include "../utilities/util.h"
#include "ethernet.h"

uint8_t arp_add_header(uint8_t* data, uint32_t* write_off,uint16_t opcode, uint8_t* dst_mac,uint32_t dst_ip){
    if (*write_off < sizeof (arp_header_t)) return ARP_HDR_RET_DATA_OVERFLOW;

    arp_header_t* hdr = (arp_header_t*)(data + *write_off - sizeof(arp_header_t));
    hdr->htype = switch_endian16(ARP_HTYPE_ETH);
    hdr->ptype = switch_endian16(ARP_PTYPE_IPv4);
    hdr->hlen = ARP_MAC_HLEN;
    hdr->plen = ARP_IP_PLEN;
    hdr->opcode = switch_endian16(opcode);
    memcpy(hdr->src_mac,nic_driver->mac_addr,sizeof(hdr->src_mac));
    hdr->src_ip = switch_endian32(nic_driver->ip_addr);

    if (dst_mac) memcpy(hdr->dst_mac,dst_mac,sizeof(hdr->dst_mac));
    else memset(hdr->dst_mac,0x0,sizeof(hdr->dst_mac));

    hdr->dst_ip = switch_endian32(dst_ip);

    *write_off -= sizeof(arp_header_t);

    return ARP_HDR_RET_SUCCESS;
}

void arp_send_request(uint32_t dst_ip){
    uint32_t total_size = sizeof(arp_header_t) + sizeof(ethernet_header_t);
    uint8_t* data = (uint8_t*)kmalloc(total_size);
    uint32_t write_off = total_size;

    uint8_t ret = arp_add_header(data,&write_off,ARP_OP_REQ_MAC,nullptr,dst_ip);
    if (ret != ARP_HDR_RET_SUCCESS) {warnf("Failed to add ARP header to ARP request"); goto cleanup;}

    uint8_t broadcast_mac[6] = {0xff,0xff,0xff,0xff,0xff,0xff};
    ret = ethernet_add_header(data,&write_off,broadcast_mac,ETHERTYPE_ARP);
    if (ret != ETH_HDR_RET_SUCCESS) {warnf("Failed to add ETHERNET header to ARP request"); goto cleanup;}

    nic_driver->send(data,total_size);

cleanup:
    kfree(data);
}

void arp_send_response(arp_header_t* arp_hdr){
    uint32_t total_size = sizeof(arp_header_t) + sizeof(ethernet_header_t);
    uint8_t* data = (uint8_t*)kmalloc(total_size);
    uint32_t write_off = total_size;

    uint32_t dst_ip = switch_endian32(arp_hdr->src_ip);

    uint8_t ret = arp_add_header(data,&write_off,ARP_OP_REPLY_MAC,arp_hdr->src_mac,dst_ip);
    if (ret != ARP_HDR_RET_SUCCESS) {warnf("Failed to add ARP header to ARP response"); goto cleanup;}

    ret = ethernet_add_header(data,&write_off,arp_hdr->src_mac,ETHERTYPE_ARP);
    if (ret != ETH_HDR_RET_SUCCESS) {warnf("Failed to add ETHERNET header to ARP response"); goto cleanup;}

    nic_driver->send(data,total_size);

cleanup:
    kfree(data);
}

void arp_cache_mac(arp_header_t* arp_hdr){
    arp_mac_cache_t* cache = (arp_mac_cache_t*)kmalloc(sizeof(arp_mac_cache_t));
    cache->ip_addr = switch_endian32(arp_hdr->src_ip);
    unsigned char* ip_addr_str = ipv4_to_str(cache->ip_addr);
    logf("ARP: Caching MAC for IP %s",ip_addr_str);
    log_MAC(arp_hdr->src_mac);
    kfree(ip_addr_str);
    memcpy(cache->mac,arp_hdr->src_mac,sizeof(cache->mac));

    arp_mac_cache_t* head = nic_driver->arp_cache_head;

    if (!head) nic_driver->arp_cache_head = cache;
    else {
        while(head->next) head = head->next;
        head->next = cache;
    }
}

uint8_t arp_cache_contains_ip(uint32_t ip_addr){
    arp_mac_cache_t* head = nic_driver->arp_cache_head;
    while(head){
        if (head->ip_addr == ip_addr) return 1;
        head = head->next;
    }
    return 0;
}

void arp_handle_packet(uint8_t* data, uint32_t write_off, uint32_t total_len){
    if (total_len < write_off + sizeof(arp_header_t)) return;
    arp_header_t* arp_hdr = (arp_header_t*)(data + write_off);
    uint16_t ptype = switch_endian16(arp_hdr->ptype);
    uint16_t htype = switch_endian16(arp_hdr->htype);
    if (ptype != ARP_PTYPE_IPv4) {
        warnf("Recieved non-IPv4 ARP, discarded");
        return;
    }
    if (htype != ARP_HTYPE_ETH){
        warnf("Recieved non-ETH ARP, discarded");
        return;
    }
    if (arp_hdr->hlen != ARP_MAC_HLEN || arp_hdr->plen != ARP_IP_PLEN){
        warnf("Unusable ARP hlen (%d) or plen (%d), discarded",arp_hdr->hlen,arp_hdr->plen);
        return;
    }
    
    uint32_t dst_ip = switch_endian32(arp_hdr->dst_ip);
    if (dst_ip != nic_driver->ip_addr) return; // not for me

    uint16_t opcode = switch_endian16(arp_hdr->opcode);
    switch (opcode)
    {
    case ARP_OP_REQ_MAC:
        arp_send_response(arp_hdr);
        break;
    case ARP_OP_REPLY_MAC:
        if (!arp_cache_contains_ip(switch_endian32(arp_hdr->src_ip))) arp_cache_mac(arp_hdr);
        break;
    
    default:
        warnf("Recieved unknown ARP opcode (%d)",opcode);
        break;
    }


}