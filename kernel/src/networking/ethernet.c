#include "ethernet.h"
#include "../memory/kmalloc.h"

void ethernet_handle_packet(uint8_t* data, uint32_t len){
    kfree(data);
}