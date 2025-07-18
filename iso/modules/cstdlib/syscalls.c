#include "syscalls.h"
#include "../../../src/tables/syscalls.h"

void write(unsigned int fd, const char* buffer, unsigned int count){
    asm volatile (
        "int $0x30"
        :
        : "a"(SYS_WRITE), "b"(fd), "c"(buffer), "d"(count)
        : "memory"
    );
}