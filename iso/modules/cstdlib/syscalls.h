
#ifndef INCLUDE_SYSCALLS_H
#define INCLUDE_SYSCALLS_H

#define FD_STDIN 0x0
#define FD_STDOUT 0x1
#define FD_STDERR 0x2

void write(unsigned int fd, const char* buffer, unsigned int count);

#endif