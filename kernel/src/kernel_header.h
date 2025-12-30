
#ifndef INCLUDE_KERNEL_PROCESS_H
#define INCLUDE_KERNEL_PROCESS_H

#include "processes/user_process.h"
#include "../limine-protocol/include/limine.h"

typedef struct  {
    uint64_t n_entries;
    struct limine_memmap_entry **memmap_entries; 
}limine_mmap_data_t; 

typedef struct {
    struct limine_framebuffer* framebuffer;
    uint64_t hhdm;
    limine_mmap_data_t mmap_data; 
    uint64_t boot_time;
    
}limine_data_t;

extern limine_data_t limine_data;
extern struct user_process global_kernel_process;
extern uint8_t dispatched_user_mode;
void shutdown();

#endif