
#ifndef INCLUDE_SYSCALL_DEFINES_H
#define INCLUDE_SYSCALL_DEFINES_H

#include <stdint.h>

#define SYS_EXIT 0x1
#define SYS_READ 0x2
#define SYS_WRITE 0x3
#define SYS_OPEN 0x4
#define SYS_CLOSE 0x5
#define SYS_ALLOC_PAGE 0x6
#define SYS_GETCWD 0x7
#define SYS_GETDENTS 0x8
#define SYS_CHDIR 0x9
#define SYS_RMFILE 0xa
#define SYS_SEEK 0xb
#define SYS_MKNOD 0xc
#define SYS_IOCTL 0xd 
#define SYS_MSSLEEP 0xe
#define SYS_SPAWN 0xf

#define SYSCALL_FAIL -1
#define SYSCALL_SUCCESS 0

#define PROT_NONE 0x0
#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define PROT_EXEC 0x4

#define MAP_SHARED 0x01
#define MAP_PRIVATE 0x02
#define MAP_ANONYMOUS 0x20
#define MAP_ANON MAP_ANONYMOUS

#define MAP_FD_NONE (uint32_t)-1

#endif