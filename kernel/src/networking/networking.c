#include "networking.h"
#include "../io/log.h"
#include "../drivers/NICs/82540EM.h"
#include "../drivers/PCI/pci.h"
#include "../utilities/util.h"
#include "../memory/kmalloc.h"
#include "arp.h"
#include "ip.h"
#include "../tables/interrupts.h"
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
void setup_network_driver(){
    net_interface_t* eth0 = (net_interface_t*)kmalloc(sizeof(net_interface_t));
    memcpy(eth0->name,"eth0",5);
    routing_table.n_routes = 0;

    register_timer_callback(ipv4_timer_callback,1000); // callback every second

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
                    eth0->netmask = NETMASK_DEFAULT;
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
    register_route(eth0,IP_TESTING & NETMASK_DEFAULT,NETMASK_DEFAULT,0); // route for local network
    register_route(eth0,0,0,ROUTER_IP); // default route (needs to be last in the routing table)

    uint32_t ip = ipv4_to_uint32("192.168.100.1"); 
    arp_send_request(ip);
}

uint16_t compute_checksum(uint8_t* hdr,uint32_t len){
    uint32_t sum = 0;
    for (uint32_t i = 0; i < len; i+= 2){
        uint16_t word = hdr[i];
        if (i+1 < len) word |= hdr[i+1]<<8;
        sum += word;
    }

    while(sum >> 16){
        sum = (sum & 0xffff) + (sum >> 16);
    }

    return (uint16_t)~sum;
}