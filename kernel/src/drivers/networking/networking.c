#include "networking.h"
#include "../../io/log.h"
#include "82540EM.h"
#include <stdbool.h>

typedef struct {
    uint16_t vendor_id;
    uint16_t dev_id;
    void (*init_driver) ();
}ethernet_driver_t;

#define N_REGISTERED_DRIVERS 1

ethernet_driver_t registered_drivers[N_REGISTERED_DRIVERS] = {
    {.dev_id = 0x100e,.vendor_id = PCI_INTEL_VENDOR_ID,.init_driver = init_82540EM_driver}
};

void setup_network_driver(){
    pci_device_t* dev = pci_head;
    while(dev){
        if (dev->class_code == PCI_CLASS_NETWORK_CONTROLLER 
         && dev->subclass == PCI_SUBCLASS_ETHERNET_CONTROLLER){
            bool found = false;
            for (uint32_t i = 0; i < N_REGISTERED_DRIVERS;i++){
                ethernet_driver_t driver = registered_drivers[i];
                if (driver.dev_id == dev->device_id && driver.vendor_id == dev->vendor_id){
                    driver.init_driver();
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