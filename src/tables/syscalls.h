
#ifndef INCLUDE_SYSCALL_NUMBERS_H
#define INCLUDE_SYSCALL_NUMBERS_H

#include "../processes/user_process.h"

#define SYS_READ 0x2
#define SYS_WRITE 0x3
#define SYS_OPEN 0x4
#define SYS_CLOSE 0x5
#define SYS_EXIT 0x1

#define SYSCALL_FAIL -1


int sys_write(user_process_t* p,unsigned int fd, unsigned char* buf, unsigned int size);

int sys_read(user_process_t* p,unsigned int fd, unsigned char* buf, unsigned int size);

int sys_open(user_process_t* p,unsigned char* filepath, unsigned char flags);

int sys_close(user_process_t* p, unsigned int fd);

int sys_exit(user_process_t* p,interrupt_stack_frame_t* stack_frame);

#endif