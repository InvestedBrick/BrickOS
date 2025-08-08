
#ifndef INCLUDE_SYSCALLS_H
#define INCLUDE_SYSCALLS_H

#include "interrupts.h"
#include "user_process.h"

#define FD_STDIN 0x0
#define FD_STDOUT 0x1
#define FD_STDERR 0x2

int write(unsigned int fd, const char* buffer, unsigned int count);
int read(unsigned int fd, const char* buffer, unsigned int count);

int open(const char* pathname, unsigned char flags);
int close(unsigned int fd);
int exit(unsigned int error_code);
int sys_exit(user_process_t* p,interrupt_stack_frame_t* stack_frame);
#endif