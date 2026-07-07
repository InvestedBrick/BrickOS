#include "networking.h"
#include "../io/log.h"
#include "../drivers/NICs/82540EM.h"
#include "../drivers/PCI/pci.h"
#include <stdbool.h>

typedef struct {
    uint16_t vendor_id;
    uint16_t dev_id;
    void (*init_driver) (pci_device_t*);
}ethernet_driver_t;

#define N_REGISTERED_DRIVERS 1

ethernet_driver_t registered_drivers[N_REGISTERED_DRIVERS] = {
    {.dev_id = NETW_DEV_ID_82540EM,.vendor_id = PCI_INTEL_VENDOR_ID,.init_driver = init_82540EM_driver}
};

uint16_t switch_endian16(uint16_t nb) {
    return (nb>>8) | (nb<<8);
}
   
uint32_t switch_endian32(uint32_t nb) {
    return ((nb>>24)&0xff)     |
           ((nb<<8)&0xff0000)   |
           ((nb>>8)&0xff00)     |
           ((nb<<24)&0xff000000);
}

void setup_network_driver(){
    pci_device_t* dev = pci_head;
    while(dev){
        if (dev->class_code == PCI_CLASS_NETWORK_CONTROLLER 
         && dev->subclass == PCI_SUBCLASS_ETHERNET_CONTROLLER){
            bool found = false;
            for (uint32_t i = 0; i < N_REGISTERED_DRIVERS;i++){
                ethernet_driver_t driver = registered_drivers[i];
                if (driver.dev_id == dev->device_id && driver.vendor_id == dev->vendor_id){
                    driver.init_driver(dev);
                    found = true;
                    break;
                }
            }
            if (!found)
                warnf("Could not find driver for ethernet controller (dev_id=%x,vendor_id=%x)",dev->device_id,dev->vendor_id);
        }
        dev = dev->next;
    }
    
}