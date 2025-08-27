
#ifndef INCLUDE_KERNEL_PROCESS_H
#define INCLUDE_KERNEL_PROCESS_H
#include "processes/user_process.h"

extern user_process_t global_kernel_process;
extern uint8_t dispatched_user_mode;
void shutdown();

#endif