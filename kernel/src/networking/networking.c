#include "networking.h"
#include "../io/log.h"
#include "../drivers/NICs/82540EM.h"
#include "../drivers/PCI/pci.h"
#include <shared/util.h>
#include "../memory/kmalloc.h"
#include "../processes/kworker.h"
#include "arp.h"
#include "ip.h"
#include "udp.h"
#include "ethernet.h"
#include "../tables/timer_callbacks.h"
#include <stdbool.h>
#include "../processes/spinlocks.h"

routing_table_t routing_table;
spinlock_t routing_table_lock;

uint8_t driver_irq_vec = 0;

uint8_t get_nic_irq_vec(){
    return driver_irq_vec;
}

typedef struct {
    uint16_t vendor_id;
    uint16_t dev_id;
    uint8_t (*init_driver) (net_interface_t*,pci_device_t*);
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

net_interface_t* get_loopback_iface(){
    route_t* lo_route = route_lookup(LOOPBACK_ADDR);
    if (!lo_route){
        errorf("Whaaat? no loopback route?? what if I want to send myself some useless mail? preposterous!");
        return nullptr;
    }
    net_interface_t* lo = lo_route->iface;

    return lo;
}

void register_route(net_interface_t* iface, uint32_t network, uint32_t netmask, uint32_t gateway){
    spinlock_acquire(&routing_table_lock);
    if (routing_table.n_routes >= MAX_ROUTING_TABLE_ENTRIES) {
        warnf("Routing table is full, cannot register new route");
        spinlock_release(&routing_table_lock);
        return;
    }
    route_t* new_route = &routing_table.routes[routing_table.n_routes];
    new_route->network = network;
    new_route->netmask = netmask;
    new_route->gateway = gateway;
    new_route->iface = iface;
    routing_table.n_routes++;
    spinlock_release(&routing_table_lock);
}

void update_or_insert_route(net_interface_t* iface, uint32_t network, uint32_t netmask, uint32_t gateway) {
    spinlock_acquire(&routing_table_lock); /* regular spinlock is safe here since we either expect it to be fine the first time (at setup) or be called from a kthread context*/
    for (uint32_t i = 0; i < routing_table.n_routes; i++) {
        route_t* r = &routing_table.routes[i];
        if (r->network == network && r->netmask == netmask) {
            r->gateway = gateway;
            r->iface = iface;
            spinlock_release(&routing_table_lock);
            return;
        }
    }
    spinlock_release(&routing_table_lock);
    register_route(iface, network, netmask, gateway);
}

void fully_update_route(net_interface_t* iface, 
                        uint32_t old_network,
                        uint32_t old_netmask,
                        uint32_t new_network,
                        uint32_t new_netmask,
                        uint32_t new_gateway){

    spinlock_acquire(&routing_table_lock); 
    for (uint32_t i = 0; i < routing_table.n_routes; i++) {
        route_t* r = &routing_table.routes[i];
        if (r->network == old_network && r->netmask == old_netmask) {
            r->network = new_network;
            r->netmask = new_netmask;
            r->gateway = new_gateway;
            r->iface = iface;
            spinlock_release(&routing_table_lock);
            return;
        }
    }
    spinlock_release(&routing_table_lock);
}

net_interface_t* init_loopback_interface(){
    net_interface_t* lo = (net_interface_t*)kmalloc(sizeof(net_interface_t));
    lo->dhcp = (dhcp_client_t*)kmalloc(sizeof(dhcp_client_t));
    memcpy(lo->name,"loopback",sizeof("loopback"));
    lo->dhcp->state = DHCP_BOUND;
    lo->send = ethernet_loopback_stub;
    lo->mtu = (uint32_t)-1;
    lo->arp_cache_head = nullptr;
    spinlock_init(&lo->mac_cache_lock); 

    return lo;
}

net_interface_t* init_eth0_interface(){
    net_interface_t* eth0 = (net_interface_t*)kmalloc(sizeof(net_interface_t));
    eth0->dhcp = (dhcp_client_t*)kmalloc(sizeof(dhcp_client_t));
    eth0->dhcp->state = DHCP_INIT;
    memcpy(eth0->name,"eth0",sizeof("eth0"));
    eth0->arp_cache_head = nullptr;
    spinlock_init(&eth0->mac_cache_lock);

    return eth0;
}

void setup_network_driver(){
    
    net_interface_t* eth0 = init_eth0_interface();    
    net_interface_t* lo = init_loopback_interface();

    spinlock_init(&routing_table_lock);
    routing_table.n_routes = 0;

    register_timer_callback(ipv4_timer_callback,1000); // callback every second
    register_timer_callback(arp_timer_callback,1000);
    init_ip_linked_lists();
    init_socket_queues();
    uint32_t f = spinlock_acquire_irq(&pci_list_lock);
    pci_device_t* dev = pci_head;
    while(dev){
        if (dev->class_code == PCI_CLASS_NETWORK_CONTROLLER 
         && dev->subclass == PCI_SUBCLASS_ETHERNET_CONTROLLER){
            bool found = false;
            for (uint32_t i = 0; i < N_REGISTERED_DRIVERS;i++){
                driver_dev_t driver = registered_drivers[i];
                if (driver.dev_id == dev->device_id && driver.vendor_id == dev->vendor_id){
                    driver_irq_vec = driver.init_driver(eth0,dev);

                    found = true;
                    break;
                }
            }
            if (!found)
                warnf("Could not find driver for ethernet controller (dev_id=%x,vendor_id=%x)",dev->device_id,dev->vendor_id);
        }
        dev = dev->next;
    }
    spinlock_release_irq(&pci_list_lock,f);
    
    logf("set up NIC driver");
    memcpy(lo->mac_addr,eth0->mac_addr,sizeof(lo->mac_addr));

    // for sending on the same machine
    register_route(lo,LOOPBACK_ADDR,0xffffffff,0); 
    arp_cache_mac_permanent_to_iface(lo,LOOPBACK_ADDR, lo->mac_addr);


    // local routes and gateway route gets configured in DHCP setup
    register_route(eth0,IP_BROADCAST_ADDRESS,0xffffffff,0);

    uint8_t broadcast_mac[6]  = {0xff,0xff,0xff,0xff,0xff,0xff};
    arp_cache_mac_permanent_to_iface(eth0,IP_BROADCAST_ADDRESS,broadcast_mac);
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

void finish_up_networking(){
    enqueue_kernel_work(dhcp_configure_ip,nullptr);
}