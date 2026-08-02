#include "networking.h"
#include "../io/log.h"
#include "../drivers/NICs/82540EM.h"
#include "../drivers/PCI/pci.h"
#include "../utilities/util.h"
#include "../memory/kmalloc.h"
#include "arp.h"
#include "ip.h"
#include "udp.h"
#include "ethernet.h"
#include "../tables/timer_callbacks.h"
#include <stdbool.h>
routing_table_t routing_table;

typedef struct {
    uint16_t vendor_id;
    uint16_t dev_id;
    void (*init_driver) (net_interface_t*,pci_device_t*);
}driver_dev_t;

#define N_REGISTERED_DRIVERS 1

driver_dev_t registered_drivers[N_REGISTERED_DRIVERS] = {
    {.dev_id = NETW_DEV_ID_82540EM,.vendor_id = PCI_INTEL_VENDOR_ID,.init_driver = init_82540EM_driver}
};

uint16_t switch_endian16(uint16_t nb) {
    return (nb>>8) | (nb<<8);
}
   
uint32_t switch_endian32(uint32_t nb) {
    return ((nb>>24)&0xff)      |
           ((nb<<8)&0xff0000)   |
           ((nb>>8)&0xff00)     |
           ((nb<<24)&0xff000000);
}
void register_route(net_interface_t* iface, uint32_t network, uint32_t netmask, uint32_t gateway){
    if (routing_table.n_routes >= MAX_ROUTING_TABLE_ENTRIES) {
        warnf("Routing table is full, cannot register new route");
        return;
    }
    route_t* new_route = &routing_table.routes[routing_table.n_routes];
    new_route->network = network;
    new_route->netmask = netmask;
    new_route->gateway = gateway;
    new_route->iface = iface;
    routing_table.n_routes++;
}

void update_or_insert_route(net_interface_t* iface, uint32_t network, uint32_t netmask, uint32_t gateway) {
    for (uint32_t i = 0; i < routing_table.n_routes; i++) {
        route_t* r = &routing_table.routes[i];
        if (r->network == network && r->netmask == netmask) {
            r->gateway = gateway;
            r->iface = iface;
            return;
        }
    }
    register_route(iface, network, netmask, gateway);
}

net_interface_t* init_loopback_interface(){
    net_interface_t* lo = (net_interface_t*)kmalloc(sizeof(net_interface_t));
    memcpy(lo->name,"loopback",sizeof("loopback"));
    lo->ip_addr = IP_TESTING;
    lo->send = ethernet_loopback_stub;
    lo->mtu = (uint32_t)-1;
    lo->arp_cache_head = (arp_mac_cache_t*)kmalloc(sizeof(arp_mac_cache_t));
    lo->arp_cache_head->next = nullptr;
    lo->arp_cache_head->timeout = ARP_CACHE_DONT_TIMEOUT;
    mutex_init(&lo->mac_cache_mutex); 

    return lo;
}

net_interface_t* init_eth0_interface(){
    net_interface_t* eth0 = (net_interface_t*)kmalloc(sizeof(net_interface_t));
    memcpy(eth0->name,"eth0",sizeof("eth0"));
    eth0->arp_cache_head = nullptr;
    mutex_init(&eth0->mac_cache_mutex);

    return eth0;
}

void setup_network_driver(){
    
    net_interface_t* eth0 = init_eth0_interface();    
    net_interface_t* lo = init_loopback_interface();

    routing_table.n_routes = 0;

    register_timer_callback(ipv4_timer_callback,1000); // callback every second
    register_timer_callback(arp_timer_callback,1000);
    init_ip_linked_lists();
    init_udp_sock_queue();
    pci_device_t* dev = pci_head;
    while(dev){
        if (dev->class_code == PCI_CLASS_NETWORK_CONTROLLER 
         && dev->subclass == PCI_SUBCLASS_ETHERNET_CONTROLLER){
            bool found = false;
            for (uint32_t i = 0; i < N_REGISTERED_DRIVERS;i++){
                driver_dev_t driver = registered_drivers[i];
                if (driver.dev_id == dev->device_id && driver.vendor_id == dev->vendor_id){
                    driver.init_driver(eth0,dev);

                    eth0->ip_addr = IP_TESTING;
                    found = true;
                    break;
                }
            }
            if (!found)
                warnf("Could not find driver for ethernet controller (dev_id=%x,vendor_id=%x)",dev->device_id,dev->vendor_id);
        }
        dev = dev->next;
    }
    logf("set up NIC driver");
    memcpy(lo->mac_addr,eth0->mac_addr,sizeof(lo->mac_addr));
    lo->arp_cache_head->ip_addr = eth0->ip_addr;

    // for sending on the same machine
    register_route(lo,IP_TESTING,0xffffffff,0);
    register_route(lo,LOOPBACK_ADDR,0xffffffff,0); 

    register_route(eth0,IP_TESTING & NETMASK_DEFAULT,NETMASK_DEFAULT,0); // route for local network
    register_route(eth0,0,0,ROUTER_IP); // default route 

    uint32_t ip = ipv4_to_uint32("192.168.100.1"); 
    arp_send_request(ip);

}

uint32_t checksum_accumulate(uint8_t* data, size_t len, uint32_t sum) {
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

uint16_t compute_udp_checksum(udp_header_t* udp_hdr, pseudo_ip_hdr_t* pseudo_ip_hdr, uint8_t* data, uint32_t data_len){
    uint32_t sum = 0;
    sum = checksum_accumulate((uint8_t*)udp_hdr,sizeof(udp_header_t),sum);
    sum = checksum_accumulate((uint8_t*)pseudo_ip_hdr,sizeof(pseudo_ip_hdr_t),sum);
    sum = checksum_accumulate((uint8_t*)data,data_len,sum);

    return checksum_finalise(sum);
}