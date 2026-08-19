
#ifndef INCLUDE_SYSCALL_NUMBERS_H
#define INCLUDE_SYSCALL_NUMBERS_H

#include "../processes/process.h"
#include "interrupts.h"
#include <shared/syscall_defines.h>
#include "../filesystem/filesystem.h"
#include <shared/network_defines.h>

// only internally used
#define MMAP_UNSPEC_ADDR (uint64_t)0x12345

uint64_t sys_write(process_t* p,uint32_t fd, unsigned char* buf, uint32_t size);

uint64_t sys_read(process_t* p,uint32_t fd, unsigned char* buf, uint32_t size);

uint64_t sys_open(process_t* p,unsigned char* filepath, uint8_t flags);

uint64_t sys_close(process_t* p, uint32_t fd);

uint64_t sys_exit(process_t* p,interrupt_stack_frame_t* stack_frame);

uint64_t sys_seek(process_t* p,uint32_t fd, uint32_t offset,uint32_t whence);

uint64_t sys_mmap(process_t *p, uint64_t addr, uint64_t size,uint32_t prot, uint32_t flags, uint32_t fd, uint64_t offset);

uint64_t sys_munmap(process_t* p, uint64_t addr, uint64_t size);

uint64_t sys_getcwd(unsigned char* buffer, uint32_t buf_len);

uint64_t sys_getdents(process_t* p,uint32_t fd,dirent_t* ent_buffer,uint32_t size);

uint64_t sys_chdir(unsigned char* dir_name);

uint64_t sys_rmfile(unsigned char* filename);

uint64_t sys_mknod(unsigned char* filename,mknod_params_t* params);

uint64_t sys_ioctl(process_t* p, uint32_t fd,uint32_t cmd, void* arg);

uint64_t sys_mssleep(interrupt_stack_frame_t* stack_frame, uint32_t time);

uint64_t sys_spawn(unsigned char* filename, unsigned char* argv[],process_fds_init_t* start_fds);

uint64_t sys_getpid(process_t* p);

uint64_t sys_gettimeofday();

uint64_t sys_settimezone(process_t* p,int utc_timezone);

uint64_t sys_socket(process_t* p, uint32_t domain, uint32_t sock_type, uint8_t protocol);

uint64_t sys_bind(process_t* p, uint32_t fd, sockaddr_t* sock_addr, uint32_t sock_addr_len);

uint64_t sys_recvfrom(process_t* p, uint32_t fd, void* buf, uint32_t buf_len, uint32_t flags, sockaddr_t* src_addr, uint32_t* addr_len);

uint64_t sys_sendto(process_t* p, uint32_t fd, void* buf, uint32_t buf_len, uint32_t flags, sockaddr_t* dst_addr, uint32_t addr_len);

uint64_t sys_setsockopt(process_t* p, uint32_t fd, uint32_t level, uint32_t optname, void* optval, uint32_t optlen);

uint64_t sys_getsockopt(process_t* p, uint32_t fd, uint32_t level, uint32_t optname, void* optval, uint32_t* optlen);
#endif