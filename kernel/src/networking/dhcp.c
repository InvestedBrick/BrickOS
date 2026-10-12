#include "dhcp.h"
#include "ip.h"
#include "../io/log.h"
#include "../memory/kmalloc.h"
#include "../tables/syscalls.h"
#include "../utilities/random.h"
#include "../utilities/vector.h"
#include "arp.h"
#include "../ACPI/apic.h"
#include "../tables/timer_callbacks.h"
#include "../processes/spinlocks.h"
#include <shared/util.h>

static vector_t dhcp_ifaces_vec;
spinlock_t dhcp_ifaces_vec_lock;

static void dhcp_lease_renewer();

uint8_t dhcp_write_header(net_interface_t* iface, 
                          uint8_t* data, 
                          uint32_t xid, 
                          uint8_t recv_style, 
                          uint32_t gateway_ip_addr, 
                          uint32_t client_ip_addr, 
                          uint32_t server_ip_addr, 
                          queue_t* options) {
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

uint8_t dhcp_discover(process_t* p, uint64_t sock_fd, net_interface_t* iface, uint32_t* xid){
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
    
    *xid = simple_rand_u32();
    
    uint8_t ret = dhcp_write_header(iface,data,*xid,DHCP_RECV_STYLE_UNICAST,0,0,0,&dhcp_options);
    
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
    return DHCP_RET_SUCCESS;

generic_fail:
    kfree(data);
    return DHCP_RET_FAILURE;

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

dhcp_ret_code dhcp_request(process_t* p, 
                           uint64_t sock_fd,
                           uint32_t xid,
                           net_interface_t* iface, 
                           dhcp_option_info_t* info,
                           bool broadcast){

    in_sockaddr_t dst;

    dst.inet_family = INET_FAM_IPv4;
    dst.inet_port = DHCP_SERVER_PORT;

    dst.inet_addr = broadcast ? IP_BROADCAST_ADDRESS : info->server_ip_addr;;

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

    /**
     * @note regarding the passing of info->ip_addr:
     * it is not entirely correct for initial setup but ignored in practices and needed when rebinding
     */
    uint8_t ret = dhcp_write_header(iface,data,xid,DHCP_RECV_STYLE_UNICAST,0,info->ip_addr,info->server_ip_addr,&dhcp_options);
    
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
    return DHCP_RET_SUCCESS;

generic_fail:
    kfree(data);
    return DHCP_RET_FAILURE;
}

dhcp_ret_code dhcp_await_ack(process_t* p, uint64_t sock_fd, uint32_t xid, dhcp_option_info_t* offer_info){
    uint8_t* recv_buf = nullptr;
    uint32_t recvd_bytes = dhcp_recv_packet(p,sock_fd,xid,&recv_buf);
    if (!recv_buf) return DHCP_RET_FAILURE;

    dhcp_option_info_t* ack_info = dhcp_extract_options(DHCPACK,(dhcp_header_t*)(recv_buf),recvd_bytes);
    if (!ack_info) return DHCP_RET_FAILURE;

    
    
    if (ack_info->server_ip_addr != offer_info->server_ip_addr){
        warnf("Inconsistent DHCP server while requesting IP address: %x (offer) : %x (ack)", offer_info->server_ip_addr, ack_info->server_ip_addr);
    }
    
    offer_info->ip_addr = ack_info->ip_addr; 
    offer_info->lease_time = ack_info->lease_time;
    
    if ((ack_info->router_addr != offer_info->router_addr) || (ack_info->subnet_mask != offer_info->subnet_mask) ){
        warnf("Inconsistent network information from DHCP while requesting IP address: router: %x (offer) : %x (ack), subnet: %x (offer) : %x (ack)", offer_info->router_addr, ack_info->router_addr, offer_info->subnet_mask, ack_info->subnet_mask);
    }

    kfree(ack_info);
    return DHCP_RET_SUCCESS;

}


dhcp_ret_code dhcp_req_ack(process_t* proc,
                           uint64_t sock_fd, 
                           uint32_t xid, 
                           net_interface_t* iface, 
                           dhcp_option_info_t* offer_info, 
                           bool broadcast){

    if( dhcp_request(proc,sock_fd,xid,iface,offer_info,broadcast) != DHCP_RET_SUCCESS){
        errorf("Failed to send DHCP request");
        return DHCP_RET_FAILURE;
    }

    if ( dhcp_await_ack(proc, sock_fd, xid, offer_info) != DHCP_RET_SUCCESS){
        errorf("Failed to recieve DHCP ACK");
        return DHCP_RET_FAILURE;
    }

    return DHCP_RET_SUCCESS;

}

void dhcp_configure_iface(net_interface_t* iface, dhcp_option_info_t* info ){
    // configure the interface with the new IP address
    iface->dhcp->ip_addr = info->ip_addr;
    iface->dhcp->server_addr = info->server_ip_addr;
    iface->dhcp->router_addr = info->router_addr;
    iface->dhcp->subnet_mask = info->subnet_mask;
    iface->dhcp->lease_end = ticks + SECS_TO_TICKS(info->lease_time);
    iface->dhcp->renewal_timestamp = ticks + SECS_TO_TICKS(info->lease_time / 2);
    iface->dhcp->rebind_timestamp = ticks + SECS_TO_TICKS(info->lease_time * 7 / 8);
    iface->dhcp->state = DHCP_BOUND;
}


void dhcp_clean_loopback(uint32_t old_ip){
    net_interface_t* lo = get_loopback_iface();
    if (!lo) return;
    arp_uncache_mac_from_iface(lo, old_ip);
}

void dhcp_configure_loopback(dhcp_option_info_t* info){
    // have loopback just use the eth0 ip address
    net_interface_t* lo = get_loopback_iface();
    if (!lo) return;

    lo->dhcp->ip_addr = info->ip_addr;
    register_route(lo,info->ip_addr,0xffffffff,0);
    lo->arp_cache_head->ip_addr = info->ip_addr;
    arp_cache_mac_permanent_to_iface(lo,info->ip_addr,lo->mac_addr);
}

void dhcp_configure_ip(){
    process_t* curr_proc = get_current_process();
    if (curr_proc->process_id != global_kernel_process.process_id){
        error("Invalid process tried to configure ip");
        return;
    }

    init_vector(&dhcp_ifaces_vec);
    spinlock_init(&dhcp_ifaces_vec_lock);
    
    uint64_t sock_fd = sys_socket(curr_proc,SOCKET_INET,SOCKET_TYPE_DGRAM,IP_PROTOCOL_UDP);

    in_sockaddr_t addr;
    addr.inet_family = INADDR_ANY;
    addr.inet_family = INET_FAM_IPv4;
    addr.inet_port = DHCP_CLIENT_PORT;
    

    if (sock_fd == SYSCALL_FAIL){
        error("Failed to open dhcp kernel socket");
        return;
    }

    if (sys_bind(curr_proc,sock_fd,(sockaddr_t*)&addr,sizeof(addr)) == (uint64_t)SYSCALL_FAIL){
        error("Failed to bind dhcp kernel socket");
        sys_close(curr_proc,sock_fd);
        return;
    }

    route_t* route = route_lookup(IP_BROADCAST_ADDRESS);
    if (!route){
        error("Failed to get route for DHCP broadcast");
        return;
    }
    
    route->iface->dhcp->state = DHCP_SELECTING;
    uint32_t xid = 0;
    if (dhcp_discover(curr_proc,sock_fd,route->iface,&xid) != DHCP_RET_SUCCESS){
        errorf("Failed to send DHCP discover");
        return;
    }

    dhcp_option_info_t* info = dhcp_await_offer(curr_proc,sock_fd,xid);
    if (!info) {
        errorf("Failed to recieve DHCP offer");
        return;
    }

    /* ah yes, using a route to register a route (but it works) */
    register_route(route->iface,info->router_addr & info->subnet_mask, info->subnet_mask,0); /* local network route */
    register_route(route->iface,0, 0,info->router_addr); /* for outside of local network */
    
    route->iface->dhcp->state = DHCP_REQUESTING;
    
    if (dhcp_req_ack(curr_proc,sock_fd,xid,route->iface,info,false) != DHCP_RET_SUCCESS){
        kfree(info);
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

    dhcp_configure_iface(route->iface,info);
    route->iface->dhcp->sock_fd = sock_fd;
    spinlock_acquire(&dhcp_ifaces_vec_lock);
    vector_append(&dhcp_ifaces_vec,(vector_data_t)route->iface);
    spinlock_release(&dhcp_ifaces_vec_lock);

    // loopback gets managed through eth0 iface
    dhcp_configure_loopback(info);

    register_timer_callback(dhcp_lease_renewer,(info->lease_time * 1000) / 2);

    kfree(info->dns_arr);
    kfree(info);

}

static void dhcp_lease_renewer(){
    unregister_timer_callback(dhcp_lease_renewer);
    for (uint32_t i = 0; i < dhcp_ifaces_vec.size; i++){
        net_interface_t* iface = (net_interface_t*)dhcp_ifaces_vec.data[i];

        if (!(iface && iface->dhcp && iface->dhcp->state == DHCP_BOUND && ticks >= iface->dhcp->renewal_timestamp)) continue;

        process_t* curr_proc = get_current_process(); /* impossible to not be root proc */
        uint32_t xid = simple_rand_u32();
        
        dhcp_option_info_t old_info;
        old_info.ip_addr = iface->dhcp->ip_addr;
        old_info.server_ip_addr = iface->dhcp->server_addr;
        old_info.router_addr = iface->dhcp->router_addr;
        old_info.subnet_mask = iface->dhcp->subnet_mask;

        if (ticks >= iface->dhcp->lease_end){
            /* need full DORA procedure again*/
            dhcp_clean_loopback(old_info.ip_addr);
            iface->dhcp->state = DHCP_SELECTING;
            uint32_t xid = 0;
            if (dhcp_discover(curr_proc,iface->dhcp->sock_fd,iface,&xid) != DHCP_RET_SUCCESS){
                errorf("Failed to send DHCP discover when renewing license for interface: %s", iface->name);
                continue;
            }
        
            dhcp_option_info_t* info = dhcp_await_offer(curr_proc,iface->dhcp->sock_fd,xid);
            if (!info) {
                errorf("Failed to recieve DHCP offer when renewing license for interface: %s",iface->name);
                continue;
            }
            fully_update_route(iface,
                               old_info.router_addr & old_info.subnet_mask, 
                               old_info.subnet_mask, 
                               info->router_addr & info->subnet_mask,
                               info->subnet_mask,
                               0);
            update_or_insert_route(iface,
                                   0,
                                   0,
                                   info->router_addr);
            
            iface->dhcp->state = DHCP_REQUESTING;

            if (dhcp_req_ack(curr_proc,iface->dhcp->sock_fd,xid,iface,info,false) != DHCP_RET_SUCCESS){
                errorf("Failed to get a new DHCP license for net interface: %s", iface->name);
                kfree(info);
                continue;
            }

            iface->dhcp->state = DHCP_BOUND;
            dhcp_configure_loopback(info);
            register_timer_callback(dhcp_lease_renewer,(info->lease_time * 1000) / 2);

            kfree(info->dns_arr);
            kfree(info);
        }
        else {
            bool broadcast = ticks >= iface->dhcp->rebind_timestamp; /* when rebind -> broadcast, otherwise unicast*/
            
            iface->dhcp->state = DHCP_REBINDING;
            
            if (dhcp_req_ack(curr_proc,iface->dhcp->sock_fd,xid,iface,&old_info,broadcast) != DHCP_RET_SUCCESS){ 
                errorf("Failed to renew DHCP license for net interface: %s",iface->name);
                register_timer_callback(dhcp_lease_renewer,30 * 1000); /* add callback every 30 seconds until it works or we time out entirely*/
                continue;
            }

            iface->dhcp->state = DHCP_BOUND;
            
            dhcp_configure_iface(iface,&old_info); 
            register_timer_callback(dhcp_lease_renewer,(old_info.lease_time * 1000) / 2);
        }
    }
}