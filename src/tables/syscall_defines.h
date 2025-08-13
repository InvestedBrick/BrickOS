
#ifndef INCLUDE_SYSCALL_DEFINES_H
#define INCLUDE_SYSCALL_DEFINES_H

#define SYS_EXIT 0x1
#define SYS_READ 0x2
#define SYS_WRITE 0x3
#define SYS_OPEN 0x4
#define SYS_CLOSE 0x5
#define SYS_ALLOC_PAGE 0x6
#define SYS_GETCWD 0x7
#define SYS_GETDENTS 0x8

#define SYSCALL_FAIL -1

#endif