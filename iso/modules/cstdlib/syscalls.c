#include "syscalls.h"
#include "../../../src/tables/syscalls.h"

int write(unsigned int fd, const char* buffer, unsigned int count){
    asm volatile (
        "int $0x30"
        :
        : "a"(SYS_WRITE), "b"(fd), "c"(buffer), "d"(count)
        : "memory"
    );
}

int read(unsigned int fd, const char* buffer, unsigned int count){
    asm volatile (
        "int $0x30"
        :
        : "a"(SYS_READ), "b"(fd), "c"(buffer), "d"(count)
        : "memory"
    );
}

int open(const char* pathname, unsigned char flags){
    asm volatile (
        "int $0x30"
        :
        : "a"(SYS_OPEN), "b"(pathname), "c"(flags)
        : "memory"
    );
}

int close(unsigned int fd){
    asm volatile (
        "int $0x30"
        :
        : "a"(SYS_CLOSE), "b"(fd)
        : "memory"
    );
}
