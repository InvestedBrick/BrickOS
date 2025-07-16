#include "syscalls.h"
#include "../../../src/tables/syscalls.h"

void interrupt(){
    asm volatile ("int 0x30");
}

void write(unsigned int fd, const char* buffer, unsigned int count){
    asm volatile (
        "movl %0, %%eax\n\t"  // syscall number for write
        "movl %1, %%ebx\n\t"  // file descriptor
        "movl %2, %%ecx\n\t"  // buffer pointer
        "movl %3, %%edx\n\t"  // count
        : // no output
        : "r"(SYS_WRITE), "r"(fd), "r"(buffer), "r"(count)
        : "%eax", "%ebx", "%ecx", "%edx"
    );

    interrupt();
}