
#ifndef INCLUDE_SYSCALL_NUMBERS_H
#define INCLUDE_SYSCALL_NUMBERS_H

#include "../processes/user_process.h"
#include "interrupts.h"

#include "../filesystem/filesystem.h"

int sys_write(user_process_t* p,unsigned int fd, unsigned char* buf, unsigned int size);

int sys_read(user_process_t* p,unsigned int fd, unsigned char* buf, unsigned int size);

int sys_open(user_process_t* p,unsigned char* filepath, unsigned char flags);

int sys_close(user_process_t* p, unsigned int fd);

int sys_exit(user_process_t* p,interrupt_stack_frame_t* stack_frame);

int sys_mmap(user_process_t* p,unsigned int size);

int sys_getcwd(unsigned char* buffer, unsigned int buf_len);

int sys_getdents(user_process_t* p,unsigned int fd,dirent_t* ent_buffer,unsigned int size);

int sys_chdir(unsigned char* dir_name);

#endif