#include "syscalls.h"
#include "fs.h"

int write(uint32_t fd, const char* buffer, uint32_t count) {
    int ret;
    asm volatile (
        "int $0x30"
        : "=a"(ret)
        : "a"(SYS_WRITE), "b"(fd), "c"(buffer), "d"(count)
        : "memory"
    );
    return ret;
}

int read(uint32_t fd, const char* buffer, uint32_t count) {
    int ret;
    asm volatile (
        "int $0x30"
        : "=a"(ret)
        : "a"(SYS_READ), "b"(fd), "c"(buffer), "d"(count)
        : "memory"
    );
    return ret;
}

int open(const char* pathname, uint8_t flags) {
    int ret;
    asm volatile (
        "int $0x30"
        : "=a"(ret)
        : "a"(SYS_OPEN), "b"(pathname), "c"(flags)
        : "memory"
    );
    return ret;
}

int close(uint32_t fd) {
    int ret;
    asm volatile (
        "int $0x30"
        : "=a"(ret)
        : "a"(SYS_CLOSE), "b"(fd)
        : "memory"
    );
    return ret;
}

int exit(uint32_t error_code) {
    int ret;
    asm volatile (
        "int $0x30"
        : "=a"(ret)
        : "a"(SYS_EXIT), "b"(error_code)
        : "memory"
    );
    return ret; // does not return, just convention
}

void* mmap(uint32_t size, uint32_t prot, uint32_t flags,
           uint32_t fd, uint32_t offset) {
    void* ret;
    asm volatile (
        "int $0x30"
        : "=a"(ret)
        : "a"(SYS_ALLOC_PAGE), "b"(size), "c"(prot),
          "d"(flags), "D"(fd), "S"(offset)
        : "memory"
    );
    return ret;
}

void getcwd(unsigned char* buffer, uint32_t size) {
    asm volatile (
        "int $0x30"
        : 
        : "a"(SYS_GETCWD), "b"(buffer), "c"(size)
        : "memory"
    );
}

int getdents(uint32_t fd, dirent_t* buffer, uint32_t size) {
    int ret;
    asm volatile (
        "int $0x30"
        : "=a"(ret)
        : "a"(SYS_GETDENTS), "b"(fd), "c"(buffer), "d"(size)
        : "memory"
    );
    return ret;
}

int chdir(unsigned char* dir_name) {
    int ret;
    asm volatile (
        "int $0x30"
        : "=a"(ret)
        : "a"(SYS_CHDIR), "b"(dir_name)
        : "memory"
    );
    return ret;
}

int rmfile(unsigned char* filename) {
    int ret;
    asm volatile (
        "int $0x30"
        : "=a"(ret)
        : "a"(SYS_RMFILE), "b"(filename)
        : "memory"
    );
    return ret;
}

int seek(uint32_t fd,uint32_t offset){
    int ret;
    asm volatile (
        "int $0x30"
        : "=a"(ret)
        : "a"(SYS_SEEK), "b"(fd), "c"(offset)
        : "memory"
    );
    return ret;
}

int ioctl(uint32_t fd, uint32_t cmd,void* arg){
    int ret;
    asm volatile (
        "int $0x30"
        : "=a"(ret)
        : "a"(SYS_IOCTL), "b"(fd), "c"(cmd), "d"(arg)
        : "memory"
    );
    return ret;
}

int mssleep(uint32_t time){
    int ret;
    asm volatile (
        "int $0x30"
        : "=a"(ret)
        : "a"(SYS_MSSLEEP), "b"(time)
        : "memory"
    );
    return ret;
}

int spawn(unsigned char* filename, unsigned char* argv[],process_fds_init_t* start_fds){
    int ret;
    asm volatile (
        "int $0x30"
        : "=a"(ret)
        : "a"(SYS_SPAWN), "b"(filename), "c"(argv) , "d"(start_fds)
        : "memory"
    );
    return ret;
}

int mknod(unsigned char* filename, uint32_t type){
    int ret;
    asm volatile (
        "int $0x30"
        : "=a"(ret)
        : "a"(SYS_MKNOD), "b"(filename), "c"(type)
        : "memory"
    );
    return ret;
}

int getpid(){
    int ret;
    asm volatile (
        "int $0x30"
        : "=a"(ret)
        : "a"(SYS_GETPID)
        : "memory"
    );
    return ret;
}