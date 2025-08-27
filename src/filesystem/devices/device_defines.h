#ifndef INCLUDE_DEVICE_COMMANDS_H
#define INCLUDE_DEVICE_COMMANDS_H
#include <stdint.h>

// these defines / structs are accessible for the userspace
#define DEV_FB0_GET_METADATA 0x01

typedef struct {
    uint32_t phys_addr;
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t bytes_per_row;
    uint32_t size;
}framebuffer_t;

#endif