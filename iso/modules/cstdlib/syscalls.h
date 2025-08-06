
#ifndef INCLUDE_SYSCALLS_H
#define INCLUDE_SYSCALLS_H

#define FD_STDIN 0x0
#define FD_STDOUT 0x1
#define FD_STDERR 0x2

int write(unsigned int fd, const char* buffer, unsigned int count);
int read(unsigned int fd, const char* buffer, unsigned int count);

int open(const char* pathname, unsigned char flags);
int close(unsigned int fd);
int exit(unsigned int error_code);

#endif