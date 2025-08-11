#include "syscalls.h"
#include "../../../src/tables/syscall_defines.h"

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

int exit(unsigned int error_code){
    asm volatile (
        "int $0x30"
        :
        : "a"(SYS_EXIT), "b"(error_code)
        : "memory"
    );
}

void* mmap(unsigned int size){
    asm volatile (
        "int $0x30"
        :
        : "a"(SYS_ALLOC_PAGE), "b"(size)
        : "memory"
    );
}

void getcwd(unsigned char* buffer, unsigned int buf_len){
    asm volatile (
        "int $0x30"
        :
        : "a"(SYS_GETCWD), "b"(buffer), "c"(buf_len)
        : "memory"
    );
}
