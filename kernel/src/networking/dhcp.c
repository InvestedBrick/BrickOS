#include "dhcp.h"
#include "ip.h"
#include "../io/log.h"
#include "../memory/kmalloc.h"
#include "../tables/syscalls.h"
#include "../utilities/random.h"

uint8_t dhcp_write_header(net_interface_t* iface, uint8_t* data, uint32_t xid, uint8_t recv_style, uint32_t gateway_ip_addr, queue_t* options) {
    if (!(options || ((dhcp_option_t*)queue_back(options))->type != OPT_END )) return DHCP_HDR_RET_INVALID_OPTIONS;
    
    dhcp_header_t* dhcp_hdr = (dhcp_header_t*)data;
    dhcp_hdr->op_code = DHCP_OP_BOOTREQUEST;
    dhcp_hdr->htype = DHCP_HTYPE_ETH;
    dhcp_hdr->hlen = DHCP_HLEN_MAC;
    dhcp_hdr->hops = 0;
    dhcp_hdr->xid = switch_endian32(xid);
    dhcp_hdr->secs = 0;
    dhcp_hdr->flags = 0;
    if (recv_style == DHCP_RECV_STYLE_BROADCAST)
        dhcp_hdr->flags |= switch_endian16(DHCP_SET_BROADCAST);

    dhcp_hdr->client_ip_addr = 0;
    dhcp_hdr->your_ip_addr   = 0;
    dhcp_hdr->server_ip_addr = 0;
    dhcp_hdr->gateway_ip_addr = switch_endian32(gateway_ip_addr);

    memcpy(dhcp_hdr->client_hardware_addr,iface->mac_addr,DHCP_HLEN_MAC);

    memset(dhcp_hdr->server_name,0x0,sizeof(dhcp_hdr->server_name));
    memset(dhcp_hdr->boot_file_name,0x0,sizeof(dhcp_hdr->boot_file_name));

    dhcp_hdr->magic_cookie = switch_endian32(DHCP_MAGIC_COOKIE);

    uint32_t write_off = sizeof(dhcp_header_t);
    while (options->size > 0){
        dhcp_option_t* option = (dhcp_option_t*)queue_pop(options);
        uint32_t total_len = 0;
        // no checking if length is valid, we trust the option provider
        if (option->type == OPT_PAD || option->type == OPT_END) total_len = 1; else total_len = option->len + 2;

        uint8_t* data_ptr = (data + write_off);
        write_off += total_len;

        memcpy(data_ptr,(void*)option,total_len);

    }

    return DHCP_HDR_RET_SUCCESS;
}

uint32_t dhcp_discover(process_t* p, uint64_t sock_fd, net_interface_t* iface){
    in_sockaddr_t dst = {
        .inet_family = INET_FAM_IPv4,
        .inet_port = DHCP_SERVER_PORT,
        .inet_addr = IP_BROADCAST_ADDRESS,
    };

    queue_t dhcp_options;
    init_queue(&dhcp_options);

    dhcp_option_t* msg_type = (dhcp_option_t*)kmalloc(sizeof(dhcp_option_t) + sizeof(uint8_t[1])); 
    dhcp_option_t* param_req_list = (dhcp_option_t*)kmalloc(sizeof(dhcp_option_t) + sizeof(uint8_t[4])); 
    dhcp_option_t* end = (dhcp_option_t*)kmalloc(sizeof(dhcp_option_t)); 

    msg_type->type = (uint8_t)OPT_DHCP_MSG_TYPE;
    msg_type->len = sizeof(uint8_t[1]);
    msg_type->data[0] = (uint8_t)DHCPDISCOVER;

    param_req_list->type = (uint8_t)OPT_PARAM_REQ_LIST;
    param_req_list->len = sizeof(uint8_t[4]);
    param_req_list->data[0] = (uint8_t)OPT_SUBNET_MASK;
    param_req_list->data[1] = (uint8_t)OPT_ROUTER;
    param_req_list->data[2] = (uint8_t)OPT_DOMAIN_NAME;
    param_req_list->data[3] = (uint8_t)OPT_DNS;

    end->type = (uint8_t)OPT_END;
    end->len = 0;

    queue_push(&dhcp_options,(queue_data_t)msg_type);
    queue_push(&dhcp_options,(queue_data_t)param_req_list);
    queue_push(&dhcp_options,(queue_data_t)end);

    
    uint32_t total_len = sizeof(dhcp_header_t) 
    + msg_type->len + sizeof(dhcp_option_t) 
    + param_req_list->len + sizeof(dhcp_option_t)
    + 1 ; // 1 for the 0xff end byte
    
    uint8_t* data = (unsigned char*)kmalloc(total_len);
    memset(data,0x0,total_len);
    
    uint32_t xid = (uint32_t)(simple_rand_u64() & 0xffffffff);
    
    uint8_t ret = dhcp_write_header(iface,data,xid,DHCP_RECV_STYLE_UNICAST,0x0,&dhcp_options);
    
    kfree(msg_type);
    kfree(param_req_list);
    kfree(end);

    if (ret != DHCP_HDR_RET_SUCCESS){
        errorf("dhcp_write_header failed with: %d",ret);
        kfree(data);
        return 0;
    }

    sys_sendto(p,sock_fd,data,total_len,0,(sockaddr_t*)&dst,sizeof(dst));

    kfree(data);
    return xid;

}

dhcp_offer_info_t* dhcp_await_offer(process_t* p, uint64_t sock_fd, uint32_t xid){

    const uint32_t recv_buf_sz = 512;
    uint8_t* recv_buf = (uint8_t*)kmalloc(recv_buf_sz);
    memset(recv_buf,0x0,recv_buf_sz);

    uint64_t ret = sys_recvfrom(p,sock_fd,recv_buf,recv_buf_sz,0,0,0);
    if (ret == SYSCALL_FAIL || ret < sizeof(dhcp_header_t)){
        errorf("Failed to recieve dhcp offer, ret: %d",ret);
        kfree(recv_buf);
        return nullptr;
    } 
    dhcp_header_t* dhcp_hdr = (dhcp_header_t*)recv_buf;
    if (dhcp_hdr->op_code != DHCP_OP_BOOTREPLY ||
        dhcp_hdr->htype   != DHCP_HTYPE_ETH    ||
        dhcp_hdr->hlen    != DHCP_HLEN_MAC     ||
        switch_endian32(dhcp_hdr->magic_cookie) != DHCP_MAGIC_COOKIE ||  
        switch_endian32(dhcp_hdr->xid) != xid){
        errorf("Invalid dhcp header when recieving offer: op_code: %d, htype: %d, hlen: %d, xid: %d (expected: %d), cookie: %x",
            dhcp_hdr->op_code,
            dhcp_hdr->htype,
            dhcp_hdr->hlen,
            switch_endian32(dhcp_hdr->xid),
            xid,
            switch_endian32(dhcp_hdr->magic_cookie)
        );
        kfree(recv_buf);
        return nullptr;
    }

    dhcp_offer_info_t* info = (dhcp_offer_info_t*)kmalloc(sizeof(dhcp_offer_info_t));
    memset(info,0x0,sizeof(dhcp_offer_info_t));

    info->ip_addr = switch_endian32(dhcp_hdr->your_ip_addr);
    info->server_ip_addr = switch_endian32(dhcp_hdr->server_ip_addr);
    
    uint32_t option_offset = 0;
    bool found_end = false;
    while(!found_end){
        if (option_offset + sizeof(dhcp_hdr) >= recv_buf_sz) break; // last line of defense

        dhcp_option_t* option = (dhcp_option_t*)((uint8_t*)dhcp_hdr + option_offset);
        switch(option->type){
            case OPT_DHCP_MSG_TYPE:
                if (option->data[0] != DHCPOFFER){
                    errorf("recieved dhcp packet is not an offer as expected...");
                    goto invalid_option;
                }
                break;
            case OPT_SUBNET_MASK:
                if (option->len != sizeof(uint32_t)) {errorf("Got invalid subnet mask len in dhcp offer"); goto invalid_option;} 
                info->subnet_mask = *(uint32_t*)option->data; // no need to switch endianness, since data is treated as individual bytes
                break;
            case OPT_ROUTER:
                if (option->len != sizeof(uint32_t)) {errorf("Got invalid router addr len in dhcp offer"); goto invalid_option;}
                info->router_addr = *(uint32_t*)option->data;
                break;
            case OPT_ADDR_LEASE_TIME:
                if (option->len != sizeof(uint32_t)) {errorf("Got invalid lease time len in dhcp offer"); goto invalid_option;}
                info->lease_time = *(uint32_t*)option->data;
                break;
            case OPT_DNS:
                if (option->len == 0 || option->len % 4 != 0) {errorf("Got invalid DNS address len in dhcp offer"); goto invalid_option;}
                info->dns_cnt = option->len / 4;
                info->dns_arr = (uint32_t*)kmalloc(option->len);
                memcpy((void*)info->dns_arr,option->data,option->len);
                break;
            case OPT_END:
                found_end = true;
                break;
            default:
                break;
        }

        option_offset += option->len + sizeof(dhcp_option_t);
        
    }

    kfree(recv_buf);
    return info;

invalid_option:
    kfree(info);
    kfree(recv_buf);
    return nullptr;

}

uint8_t dhcp_request(process_t* p, uint64_t sock_fd, uint32_t xid){
    return 0;
}

void dhcp_await_ack(process_t* p, uint64_t sock_fd, uint32_t xid){

}

void dhcp_configure_ip(){
    process_t* curr_proc = get_current_process();
    if (curr_proc->process_id != global_kernel_process.process_id){
        error("Invalid process tried to configure ip");
        return;
    }

    uint64_t sock_fd = sys_socket(curr_proc,SOCKET_INET,SOCKET_TYPE_DGRAM,IP_PROTOCOL_UDP);
    in_sockaddr_t addr = {
        .inet_addr = INADDR_ANY,
        .inet_family = INET_FAM_IPv4,
        .inet_port = DHCP_CLIENT_PORT,
    };

    if (sock_fd == SYSCALL_FAIL){
        error("Failed to open dhcp kernel socket");
        return;
    }

    if (sys_bind(curr_proc,sock_fd,(sockaddr_t*)&addr,sizeof(addr)) < 0){
        error("Failed to bind dhcp kernel socket");
        sys_close(curr_proc,sock_fd);
        return;
    }

    route_t* route = route_lookup(IP_BROADCAST_ADDRESS);
    if (!route){
        error("Failed to get route for dhcp broadcast");
        return;
    }

    uint32_t xid = dhcp_discover(curr_proc,sock_fd,route->iface);
    dhcp_offer_info_t* info = dhcp_await_offer(curr_proc,sock_fd,xid);

    uint8_t out_addr[16] = {0};
    ipv4_to_str(info->ip_addr,out_addr);
    logf("DHCP OFFER: %s", out_addr);

}