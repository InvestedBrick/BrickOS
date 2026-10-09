#include "dhcp.h"
#include "ip.h"
#include "../io/log.h"
#include "../memory/kmalloc.h"
#include "../tables/syscalls.h"
#include "../utilities/random.h"
#include "arp.h"
#include "../ACPI/apic.h"
#include <shared/util.h>

uint8_t dhcp_write_header(net_interface_t* iface, uint8_t* data, uint32_t xid, uint8_t recv_style, uint32_t gateway_ip_addr, uint32_t client_ip_addr, uint32_t server_ip_addr, queue_t* options) {
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

    dhcp_hdr->client_ip_addr = switch_endian32(client_ip_addr);
    dhcp_hdr->your_ip_addr   = 0;
    dhcp_hdr->server_ip_addr = switch_endian32(server_ip_addr);
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

uint32_t dhcp_recv_packet(process_t* p, uint64_t sock_fd, uint32_t xid, uint8_t** out_buf){
    const uint32_t recv_buf_sz = 512;
    *out_buf = (uint8_t*)kmalloc(recv_buf_sz);
    memset(*out_buf,0x0,recv_buf_sz);
    
    uint64_t recvd_bytes = sys_recvfrom(p,sock_fd,*out_buf,recv_buf_sz,0,0,0);

    if (recvd_bytes == SYSCALL_FAIL || recvd_bytes < sizeof(dhcp_header_t)){
        errorf("Failed to recieve dhcp packet, ret: %d",recvd_bytes);
        kfree(*out_buf);
        *out_buf = nullptr;
        return 0;
    } 
    dhcp_header_t* dhcp_hdr = (dhcp_header_t*)(*out_buf);
    if (dhcp_hdr->op_code != DHCP_OP_BOOTREPLY ||
        dhcp_hdr->htype   != DHCP_HTYPE_ETH    ||
        dhcp_hdr->hlen    != DHCP_HLEN_MAC     ||
        switch_endian32(dhcp_hdr->magic_cookie) != DHCP_MAGIC_COOKIE ||  
        switch_endian32(dhcp_hdr->xid) != xid){
        errorf("Recieved invalid dhcp header: op_code: %d, htype: %d, hlen: %d, xid: %d (expected: %d), cookie: %x",
            dhcp_hdr->op_code,
            dhcp_hdr->htype,
            dhcp_hdr->hlen,
            switch_endian32(dhcp_hdr->xid),
            xid,
            switch_endian32(dhcp_hdr->magic_cookie)
        );
        kfree(*out_buf);
        *out_buf = nullptr;
        return 0;
    }

    return recvd_bytes;
}

uint32_t dhcp_discover(process_t* p, uint64_t sock_fd, net_interface_t* iface){
    in_sockaddr_t dst;

    dst.inet_family = INET_FAM_IPv4;
    dst.inet_port = DHCP_SERVER_PORT;
    dst.inet_addr = IP_BROADCAST_ADDRESS;

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
    
    uint8_t ret = dhcp_write_header(iface,data,xid,DHCP_RECV_STYLE_UNICAST,0,0,0,&dhcp_options);
    
    kfree(msg_type);
    kfree(param_req_list);
    kfree(end);

    if (ret != DHCP_HDR_RET_SUCCESS){
        errorf("dhcp_write_header failed with: %d",ret);
        goto generic_fail;
    }

    uint64_t syscall_ret = sys_sendto(p,sock_fd,data,total_len,0,(sockaddr_t*)&dst,sizeof(dst));
    if (syscall_ret == (uint64_t)SYSCALL_FAIL){
        errorf("sys_Sendto failed with return code: %d",syscall_ret);
        goto generic_fail;
    }

    kfree(data);
    return xid;

generic_fail:
    kfree(data);
    return 0;

}

dhcp_option_info_t* dhcp_extract_options(dhcp_msg_type_e expected_type, dhcp_header_t* dhcp_hdr, uint32_t recvd_bytes){
    dhcp_option_info_t* info = (dhcp_option_info_t*)kmalloc(sizeof(dhcp_option_info_t));
    memset(info,0x0,sizeof(dhcp_option_info_t));

    info->ip_addr = switch_endian32(dhcp_hdr->your_ip_addr);
    info->server_ip_addr = switch_endian32(dhcp_hdr->server_ip_addr);

    uint32_t option_offset = 0;
    bool found_end = false;
    while(!found_end){
        if (option_offset + sizeof(dhcp_header_t) >= recvd_bytes) break; // last line of defense

        dhcp_option_t* option = (dhcp_option_t*)((uint8_t*)dhcp_hdr + sizeof(dhcp_header_t) + option_offset);
        switch(option->type){
            case OPT_DHCP_MSG_TYPE:
                if (option->data[0] != expected_type){
                    errorf("Recieved unexpected DHCP packet (expected %d, got %d) ",option->data[0], expected_type);
                    goto invalid_option;
                }
                break;
            case OPT_SUBNET_MASK:
                if (option->len != sizeof(uint32_t)) {errorf("Got invalid subnet mask len in dhcp packet"); goto invalid_option;} 
                info->subnet_mask = switch_endian32(*(uint32_t*)option->data); 
                break;
            case OPT_ROUTER:
                if (option->len != sizeof(uint32_t)) {errorf("Got invalid router addr len in dhcp packet"); goto invalid_option;}
                info->router_addr = switch_endian32(*(uint32_t*)option->data);
                break;
            case OPT_ADDR_LEASE_TIME:
                if (option->len != sizeof(uint32_t)) {errorf("Got invalid lease time len in dhcp packet"); goto invalid_option;}
                info->lease_time = switch_endian32(*(uint32_t*)option->data);
                break;
            case OPT_DNS:
                if (option->len == 0 || option->len % 4 != 0) {errorf("Got invalid DNS address len in dhcp packet"); goto invalid_option;}
                info->dns_cnt = option->len / 4;
                info->dns_arr = (uint32_t*)kmalloc(option->len);

                for (uint32_t i = 0; i < info->dns_cnt; i++){
                    info->dns_arr[i] = switch_endian32(*(uint32_t*)(option->data + (i * 4)));
                }
                break;
            case OPT_END:
                found_end = true;
                break;
            default:
                break;
        }

        option_offset += option->len + sizeof(dhcp_option_t);
        
    }

    return info;
invalid_option:
    if (info->dns_arr) kfree(info->dns_arr);
    kfree(info);
    return nullptr;

}

dhcp_option_info_t* dhcp_await_offer(process_t* p, uint64_t sock_fd, uint32_t xid){

    uint8_t* recv_buf = nullptr;
    uint32_t recvd_bytes = dhcp_recv_packet(p,sock_fd,xid,&recv_buf);
    if (!recv_buf) return nullptr;
    
    dhcp_option_info_t* info = dhcp_extract_options(DHCPOFFER,(dhcp_header_t*)(recv_buf),recvd_bytes);
    
    kfree(recv_buf);

    return info;

}

uint8_t dhcp_request(process_t* p, uint64_t sock_fd, uint32_t xid, net_interface_t* iface, dhcp_option_info_t* info){
    in_sockaddr_t dst;

    dst.inet_family = INET_FAM_IPv4;
    dst.inet_port = DHCP_SERVER_PORT;
    dst.inet_addr = info->server_ip_addr;

    queue_t dhcp_options;
    init_queue(&dhcp_options);

    dhcp_option_t* msg_type = (dhcp_option_t*)kmalloc(sizeof(dhcp_option_t) + sizeof(uint8_t[1])); 
    dhcp_option_t* req_ip_addr = (dhcp_option_t*)kmalloc(sizeof(dhcp_option_t) + sizeof(uint32_t));
    dhcp_option_t* dhcp_server = (dhcp_option_t*)kmalloc(sizeof(dhcp_option_t) + sizeof(uint32_t));
    dhcp_option_t* end = (dhcp_option_t*)kmalloc(sizeof(dhcp_option_t)); 

    msg_type->type = (uint8_t)OPT_DHCP_MSG_TYPE;
    msg_type->len = sizeof(uint8_t[1]);
    msg_type->data[0] = (uint8_t)DHCPREQUEST;

    req_ip_addr->type = (uint8_t)OPT_REQUESTED_IP_ADDR;
    req_ip_addr->len = sizeof(uint32_t);
    *(uint32_t*)req_ip_addr->data = switch_endian32(info->ip_addr);

    dhcp_server->type = (uint8_t)OPT_SERVER_IDENT;
    dhcp_server->len = sizeof(uint32_t);
    *(uint32_t*)dhcp_server->data = switch_endian32(info->server_ip_addr);

    end->type = (uint8_t)OPT_END;
    end->len = 0;

    queue_push(&dhcp_options,(queue_data_t)msg_type);
    queue_push(&dhcp_options,(queue_data_t)req_ip_addr);
    queue_push(&dhcp_options,(queue_data_t)dhcp_server);
    queue_push(&dhcp_options,(queue_data_t)end);

    uint32_t total_len = sizeof(dhcp_header_t)
    + msg_type->len + sizeof(dhcp_option_t)
    + req_ip_addr->len + sizeof(dhcp_option_t)
    + dhcp_server->len + sizeof(dhcp_option_t)
    + 1; // 1 for the 0xff end byte

    uint8_t* data = (unsigned char*)kmalloc(total_len);
    memset(data,0x0,total_len);

    uint8_t ret = dhcp_write_header(iface,data,xid,DHCP_RECV_STYLE_UNICAST,0,0,info->server_ip_addr,&dhcp_options);
    
    kfree(msg_type);
    kfree(req_ip_addr);
    kfree(dhcp_server);
    kfree(end);

    if (ret != DHCP_HDR_RET_SUCCESS){
        errorf("dhcp_write_header failed with: %d",ret);
        goto generic_fail;
    }
    
    uint64_t syscall_ret =  sys_sendto(p,sock_fd,data,total_len,0,(sockaddr_t*)&dst,sizeof(dst));
    if (syscall_ret == (uint64_t)SYSCALL_FAIL) {
        errorf("sys_sendto failed with return code: %d",syscall_ret);
        goto generic_fail;
    }
    kfree(data);
    return 1;

generic_fail:
    kfree(data);
    return 0;
}

uint8_t dhcp_await_ack(process_t* p, uint64_t sock_fd, uint32_t xid, dhcp_option_info_t* offer_info){
    uint8_t* recv_buf = nullptr;
    uint32_t recvd_bytes = dhcp_recv_packet(p,sock_fd,xid,&recv_buf);
    if (!recv_buf) return nullptr;

    dhcp_option_info_t* ack_info = dhcp_extract_options(DHCPACK,(dhcp_header_t*)(recv_buf),recvd_bytes);
    if (!ack_info) return nullptr;


    if (ack_info->server_ip_addr != offer_info->server_ip_addr){
        warnf("Inconsistent DHCP server while requesting IP address: %x (offer) : %x (ack)", offer_info->server_ip_addr, ack_info->server_ip_addr);
    }

    offer_info->ip_addr = ack_info->ip_addr; // formality
    
    if ((ack_info->router_addr != offer_info->router_addr) || (ack_info->subnet_mask != offer_info->subnet_mask) ){
        warnf("Inconsistent network information from DHCP while requesting IP address: router: %x (offer) : %x (ack), subnet: %x (offer) : %x (ack)", offer_info->router_addr, ack_info->router_addr, offer_info->subnet_mask, ack_info->subnet_mask);
    }

    kfree(ack_info);
    return 1;

}

void dhcp_configure_ip(){
    process_t* curr_proc = get_current_process();
    if (curr_proc->process_id != global_kernel_process.process_id){
        error("Invalid process tried to configure ip");
        return;
    }
    
    uint64_t sock_fd = sys_socket(curr_proc,SOCKET_INET,SOCKET_TYPE_DGRAM,IP_PROTOCOL_UDP);

    in_sockaddr_t addr;
    addr.inet_family = INADDR_ANY;
    addr.inet_family = INET_FAM_IPv4;
    addr.inet_port = DHCP_CLIENT_PORT;
    

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
    
    route->iface->dhcp->state = DHCP_SELECTING;

    uint32_t xid = dhcp_discover(curr_proc,sock_fd,route->iface);
    
    if (!xid){
        errorf("Failed to send DHCP discover");
        return;
    }

    dhcp_option_info_t* info = dhcp_await_offer(curr_proc,sock_fd,xid);
    if (!info) {
        errorf("Failed to recieve DHCP offer");
        return;
    }

    // ah yes, using a rout to register a route (but it works)
    register_route(route->iface,info->router_addr & info->subnet_mask, info->subnet_mask,0); // local network route
    register_route(route->iface,0, 0,info->router_addr); // for outside of local network
    
    route->iface->dhcp->state = DHCP_REQUESTING;
    if( !dhcp_request(curr_proc,sock_fd,xid,route->iface,info)){
        errorf("Failed to send DHCP request");
        return;
    }

    if ( !dhcp_await_ack(curr_proc, sock_fd, xid, info)){
        errorf("Failed to recieve DHCP ACK");
        return;
    }

    uint8_t out_addr[16];
    memset(out_addr,0x0,sizeof(out_addr));
    logf("DHCP ACK info:");
    logf("IP: %x",info->ip_addr);
    logf("Server: %x",info->server_ip_addr);
    logf("Subnet mask: %x",info->subnet_mask);
    logf("Router: %x",info->router_addr);
    logf("Lease time: %d",info->lease_time);
    logf("DNS count: %d",info->dns_cnt);
    for (uint32_t i = 0; i < info->dns_cnt; i++){
        logf("DNS %d: %x",i,info->dns_arr[i]);
    }

    // configure the interface with the new IP address
    route->iface->dhcp->ip_addr = info->ip_addr;
    route->iface->dhcp->server = info->server_ip_addr;
    route->iface->dhcp->xid = xid;
    route->iface->dhcp->lease_end = ticks + info->lease_time;
    route->iface->dhcp->renewal_timestamp = ticks + (info->lease_time / 2);
    route->iface->dhcp->rebind_timestamp = ticks + (info->lease_time * 7 / 8);
    route->iface->dhcp->state = DHCP_BOUND;


    // have loopback just use the eth0 ip address
    route_t* lo_route = route_lookup(LOOPBACK_ADDR);
    if (!lo_route){
        errorf("Whaaat? no loopback route?? what if I want to send myself some useless mail? preposterous!");
        kfree(info);
        return;
    }
    net_interface_t* lo = lo_route->iface;
    lo->dhcp->ip_addr = info->ip_addr;
    register_route(lo,info->ip_addr,0xffffffff,0);
    lo->arp_cache_head->ip_addr = info->ip_addr;
    arp_cache_mac_permanent_to_iface(lo,info->ip_addr,lo->mac_addr);

}