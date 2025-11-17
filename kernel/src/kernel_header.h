
#ifndef INCLUDE_KERNEL_PROCESS_H
#define INCLUDE_KERNEL_PROCESS_H
#include "processes/user_process.h"
#include "../../limine/limine.h"
typedef struct {
    struct limine_framebuffer* framebuffer;
    uint64_t hhdm;
}limine_data_t;

extern limine_data_t limine_data;
extern user_process_t global_kernel_process;
extern uint8_t dispatched_user_mode;
void shutdown();

#endif