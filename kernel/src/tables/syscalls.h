
#ifndef INCLUDE_SYSCALL_NUMBERS_H
#define INCLUDE_SYSCALL_NUMBERS_H

#include "../processes/user_process.h"
#include "interrupts.h"
#include "syscall_defines.h"
#include "../filesystem/filesystem.h"

uint64_t sys_write(user_process_t* p,uint32_t fd, unsigned char* buf, uint32_t size);

uint64_t sys_read(user_process_t* p,uint32_t fd, unsigned char* buf, uint32_t size);

uint64_t sys_open(user_process_t* p,unsigned char* filepath, uint8_t flags);

uint64_t sys_close(user_process_t* p, uint32_t fd);

uint64_t sys_exit(user_process_t* p,interrupt_stack_frame_t* stack_frame);

uint64_t sys_seek(user_process_t* p,uint32_t fd, uint32_t offset);

uint64_t sys_mmap(user_process_t *p, void *addr, uint32_t size,uint32_t prot, uint32_t flags, uint32_t fd, uint32_t offset);

uint64_t sys_getcwd(unsigned char* buffer, uint32_t buf_len);

uint64_t sys_getdents(user_process_t* p,uint32_t fd,dirent_t* ent_buffer,uint32_t size);

uint64_t sys_chdir(unsigned char* dir_name);

uint64_t sys_rmfile(unsigned char* filename);

uint64_t sys_mknod(unsigned char* filename,mknod_params_t* params);

uint64_t sys_ioctl(user_process_t* p, uint32_t fd,uint32_t cmd, void* arg);

uint64_t sys_mssleep(interrupt_stack_frame_t* stack_frame, uint32_t time);

uint64_t sys_spawn(unsigned char* filename, unsigned char* argv[],process_fds_init_t* start_fds);

uint64_t sys_getpid(user_process_t* p);
#endif