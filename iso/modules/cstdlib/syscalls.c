#include "syscalls.h"
#include "fs.h"

int write(uint32_t fd, const char* buffer, uint32_t count){
    asm volatile (
        "int $0x30"
        :
        : "a"(SYS_WRITE), "b"(fd), "c"(buffer), "d"(count)
        : "memory"
    );
}

int read(uint32_t fd, const char* buffer, uint32_t count){
    asm volatile (
        "int $0x30"
        :
        : "a"(SYS_READ), "b"(fd), "c"(buffer), "d"(count)
        : "memory"
    );
}

int open(const char* pathname, uint8_t flags){
    asm volatile (
        "int $0x30"
        :
        : "a"(SYS_OPEN), "b"(pathname), "c"(flags)
        : "memory"
    );
}

int close(uint32_t fd){
    asm volatile (
        "int $0x30"
        :
        : "a"(SYS_CLOSE), "b"(fd)
        : "memory"
    );
}

int exit(uint32_t error_code){
    asm volatile (
        "int $0x30"
        :
        : "a"(SYS_EXIT), "b"(error_code)
        : "memory"
    );
}

void* mmap(uint32_t size){
    asm volatile (
        "int $0x30"
        :
        : "a"(SYS_ALLOC_PAGE), "b"(size)
        : "memory"
    );
}

void getcwd(unsigned char* buffer, uint32_t size){
    asm volatile (
        "int $0x30"
        :
        : "a"(SYS_GETCWD), "b"(buffer), "c"(size)
        : "memory"
    );
}

int getdents(uint32_t fd,dirent_t* buffer,uint32_t size){
    asm volatile (
        "int $0x30"
        :
        : "a"(SYS_GETDENTS), "b"(fd), "c"(buffer), "d"(size)
        : "memory"
    );
}

int chdir(unsigned char* dir_name){
    asm volatile (
        "int $0x30"
        :
        : "a"(SYS_CHDIR), "b"(dir_name)
        : "memory"
    );
}

int rmfile(unsigned char* filename){
    asm volatile (
        "int $0x30"
        :
        : "a"(SYS_RMFILE), "b"(filename)
        : "memory"
    );
}