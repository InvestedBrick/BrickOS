
#ifndef INCLUDE_SYSCALL_NUMBERS_H
#define INCLUDE_SYSCALL_NUMBERS_H

#include "../processes/user_process.h"
#include "interrupts.h"
int sys_write(user_process_t* p,unsigned int fd, unsigned char* buf, unsigned int size);

int sys_read(user_process_t* p,unsigned int fd, unsigned char* buf, unsigned int size);

int sys_open(user_process_t* p,unsigned char* filepath, unsigned char flags);

int sys_close(user_process_t* p, unsigned int fd);

int sys_exit(user_process_t* p,interrupt_stack_frame_t* stack_frame);

int sys_mmap(user_process_t* p,unsigned int size);
#endif