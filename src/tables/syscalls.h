
#ifndef INCLUDE_SYSCALL_NUMBERS_H
#define INCLUDE_SYSCALL_NUMBERS_H

#include "../processes/user_process.h"
#include "interrupts.h"

#include "../filesystem/filesystem.h"

int sys_write(user_process_t* p,uint32_t fd, uint8_t* buf, uint32_t size);

int sys_read(user_process_t* p,uint32_t fd, uint8_t* buf, uint32_t size);

int sys_open(user_process_t* p,uint8_t* filepath, uint8_t flags);

int sys_close(user_process_t* p, uint32_t fd);

int sys_exit(user_process_t* p,interrupt_stack_frame_t* stack_frame);

int sys_mmap(user_process_t* p,uint32_t size);

int sys_getcwd(uint8_t* buffer, uint32_t buf_len);

int sys_getdents(user_process_t* p,uint32_t fd,dirent_t* ent_buffer,uint32_t size);

int sys_chdir(uint8_t* dir_name);

int sys_rmfile(uint8_t* filename);

#endif