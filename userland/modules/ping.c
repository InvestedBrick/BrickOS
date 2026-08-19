#include "cstdlib/syscalls.h"
#include "cstdlib/malloc.h"
//TODO: move as include to cstdlib
#include <shared/network_defines.h>
#include "cstdlib/stdio.h"
#include <shared/util.h>

#define PCKT_SIZE 64

char echo_msg[] = "The packet knows where it is at all times";

uint16_t switch_endian16(uint16_t nb) {
    return (nb>>8) | (nb<<8);
}
   
uint32_t switch_endian32(uint32_t nb) {
    return ((nb>>24)&0xff)      |
           ((nb<<8)&0xff0000)   |
           ((nb>>8)&0xff00)     |
           ((nb<<24)&0xff000000);
}

uint32_t checksum_accumulate(uint8_t* data, uint32_t len, uint32_t sum) {
    for (uint32_t i = 0; i < len; i += 2) {
        uint16_t word = (data[i] << 8);
        if (i + 1 < len) word |= data[i + 1];
        sum += word;
    }
    return sum;
}

uint16_t checksum_finalise(uint32_t sum){
    while(sum >> 16){
        sum = (sum & 0xffff) + (sum >> 16);
    }

    uint16_t result = (uint16_t)~sum;

    return result;
}

uint16_t compute_checksum(uint8_t* hdr,uint32_t len){
    uint32_t sum = checksum_accumulate(hdr,len,0);

    return checksum_finalise(sum);
}

typedef struct {
    icmp_header_t hdr;
    icmp_echo_t echo;
    char msg[PCKT_SIZE - (sizeof(icmp_header_t) + sizeof(icmp_echo_t))];

}ping_pckt_t;

void send_ping(int sockfd, in_sockaddr_t* dst_addr, uint16_t* seq){
    ping_pckt_t packet;
    memset(&packet,0x0,sizeof(packet));

    packet.hdr.icmp_type = ICMP_TYPE_ECHO_MSG;
    packet.echo.echo_ident = switch_endian16((uint16_t)getpid());
    packet.echo.echo_seq = switch_endian16(*seq);
    *seq += 1;

    memcpy(packet.msg,echo_msg,sizeof(echo_msg));

    packet.hdr.checksum = switch_endian16(compute_checksum((uint8_t*)&packet,sizeof(packet)));

    if (sendto(sockfd,&packet,sizeof(packet),0,(sockaddr_t*)dst_addr,sizeof(*dst_addr)) < 0){
        print("Sending packet failed...");
        return;
    }
    print("Sent packet...\n");
}

void recv_ping(int sockfd, in_sockaddr_t* src_addr){
    char rbuf [128] = {0};
    in_sockaddr_t inet_addr;
    uint32_t inet_addr_size = 0;
    int pid = getpid();
    while(true){

        if (recvfrom(sockfd,rbuf,sizeof(rbuf),0,(sockaddr_t*)&inet_addr,&inet_addr_size) < 0){
            print("Failed to recieve packet...\n");
            return;
        }
        
        ipv4_header_t* ip_hdr = (ipv4_header_t*)rbuf;
        uint8_t ip_hdr_len = (ip_hdr->version_ihl & 0xf) * sizeof(uint32_t);
        ping_pckt_t* packet = (ping_pckt_t*)(rbuf + ip_hdr_len);

        if (inet_addr.inet_addr != src_addr->inet_addr) continue; // not from who we are waiting for

        if (inet_addr_size != sizeof(in_sockaddr_t)){
            print("Recieved invalid inet header size..\n");
            return;
        }

        if (packet->hdr.icmp_type != ICMP_TYPE_ECHO_REPLY) continue;
        if (switch_endian16(packet->echo.echo_ident) != pid ) continue;

        // kernel verifies checksum beforehand, so no need to do it here again

        // for me.. yay
        uint32_t post_hdr_len = switch_endian16(ip_hdr->total_length) - ip_hdr_len - sizeof(icmp_header_t) - sizeof(icmp_echo_t); 
        printf("Recieved packet successfully, seq=%d, ttl=%d\n",switch_endian16(packet->echo.echo_seq),ip_hdr->time_to_live);
        return;
       
    }


}

void main(){
    in_sockaddr_t dst_addr;
    dst_addr.inet_family = INET_FAM_IPv4;
    dst_addr.inet_addr = 0xc0a86401;
    
    int sockfd = socket(SOCKET_INET,SOCKET_TYPE_RAW,IP_PROTOCOL_ICMP);
    if (sockfd < 0) {
        print("Failed to open socket\n");
        exit(1);
    }

    uint8_t ttl = 64;
    if (setsockopt(sockfd,SOL_IP,IP_TTL,(void*)&ttl,sizeof(ttl)) < 0){
        print("Failed to set TTL\n");
    }

    uint64_t timeout = 5000;
    if (setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEOUT,(void*)&timeout,sizeof(timeout)) < 0){
        print("Failed to set timeout\n");
    }
    uint16_t seq = 0;

    for (uint32_t i = 0; i < 5;i++){
        send_ping(sockfd,&dst_addr,&seq);
        mssleep(1000);
        recv_ping(sockfd,&dst_addr);
    }

    exit(0);
}