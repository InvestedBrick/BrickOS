#include "syscalls.h"
#include "fs.h"

int write(uint32_t fd, const char* buffer, uint32_t count) {
    int ret;
    asm volatile (
        "int $0x30"
        : "=a"(ret)
        : "a"(SYS_WRITE), "D"(fd), "S"(buffer), "d"(count)
        : "memory"
    );
    return ret;
}

int read(uint32_t fd, const char* buffer, uint32_t count) {
    int ret;
    asm volatile (
        "int $0x30"
        : "=a"(ret)
        : "a"(SYS_READ), "D"(fd), "S"(buffer), "d"(count)
        : "memory"
    );
    return ret;
}

int open(const char* pathname, uint8_t flags) {
    int ret;
    asm volatile (
        "int $0x30"
        : "=a"(ret)
        : "a"(SYS_OPEN), "D"(pathname), "S"(flags)
        : "memory"
    );
    return ret;
}

int close(uint32_t fd) {
    int ret;
    asm volatile (
        "int $0x30"
        : "=a"(ret)
        : "a"(SYS_CLOSE), "D"(fd)
        : "memory"
    );
    return ret;
}

int exit(uint32_t error_code) {
    int ret;
    asm volatile (
        "int $0x30"
        : "=a"(ret)
        : "a"(SYS_EXIT), "D"(error_code)
        : "memory"
    );
    return ret; // does not return, just convention
}

void* mmap(uint32_t size, uint32_t prot, uint32_t flags, uint32_t fd, uint32_t offset) {
    register uint64_t r10 asm("r10") = fd;
    register uint64_t r8  asm("r8")  = offset;

    void *ret;

    asm volatile (
        "int $0x30"
        : "=a"(ret)
        : "a"(SYS_MMAP),
          "D"(size),
          "S"(prot),
          "d"(flags),
          "r"(r10),
          "r"(r8)
        : "memory"
    );

    return ret;
}
void munmap(void* addr, uint64_t size){
    asm volatile (
        "int $0x30"
        :
        : "a"(SYS_MUNMAP), "D"(addr), "S"(size)
        : "memory"
    );
}

void getcwd(unsigned char* buffer, uint32_t size) {
    asm volatile (
        "int $0x30"
        : 
        : "a"(SYS_GETCWD), "D"(buffer), "S"(size)
        : "memory"
    );
}

int getdents(uint32_t fd, dirent_t* buffer, uint32_t size) {
    int ret;
    asm volatile (
        "int $0x30"
        : "=a"(ret)
        : "a"(SYS_GETDENTS), "D"(fd), "S"(buffer), "d"(size)
        : "memory"
    );
    return ret;
}

int chdir(unsigned char* dir_name) {
    int ret;
    asm volatile (
        "int $0x30"
        : "=a"(ret)
        : "a"(SYS_CHDIR), "D"(dir_name)
        : "memory"
    );
    return ret;
}

int rmfile(unsigned char* filename) {
    int ret;
    asm volatile (
        "int $0x30"
        : "=a"(ret)
        : "a"(SYS_RMFILE), "D"(filename)
        : "memory"
    );
    return ret;
}

int seek(uint32_t fd,uint32_t offset,uint32_t whence){
    int ret;
    asm volatile (
        "int $0x30"
        : "=a"(ret)
        : "a"(SYS_SEEK), "D"(fd), "S"(offset), "d"(whence)
        : "memory"
    );
    return ret;
}

int ioctl(uint32_t fd, uint32_t cmd,void* arg){
    int ret;
    asm volatile (
        "int $0x30"
        : "=a"(ret)
        : "a"(SYS_IOCTL), "D"(fd), "S"(cmd), "d"(arg)
        : "memory"
    );
    return ret;
}

int mssleep(uint32_t time){
    int ret;
    asm volatile (
        "int $0x30"
        : "=a"(ret)
        : "a"(SYS_MSSLEEP), "D"(time)
        : "memory"
    );
    return ret;
}

int spawn(unsigned char* filename, unsigned char* argv[],process_fds_init_t* start_fds){
    int ret;
    asm volatile (
        "int $0x30"
        : "=a"(ret)
        : "a"(SYS_SPAWN), "D"(filename), "S"(argv) , "d"(start_fds)
        : "memory"
    );
    return ret;
}

int mknod(unsigned char* filename, mknod_params_t* params){
    int ret;
    asm volatile (
        "int $0x30"
        : "=a"(ret)
        : "a"(SYS_MKNOD), "D"(filename), "S"(params)
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

void debug(unsigned char* msg){
    asm volatile (
        "int $0x30"
        :
        : "a"(SYS_DEBUG), "D"(msg)
        : "memory"
    );
}

uint64_t gettimeofday(){
    uint64_t ret;
    asm volatile (
        "int $0x30"
        : "=a"(ret)
        : "a"(SYS_GETTIMEOFDAY)
        : "memory"
    );
    return ret;
}

void settimezone(int utc_timezone){
    asm volatile (
        "int $0x30"
        :
        : "a"(SYS_SETTIMEZONE), "D"(utc_timezone)
        : "memory"
    );
}

int socket(uint32_t domain, uint32_t type, uint32_t protocol){
    int ret;
    asm volatile (
        "int $0x30"
        : "=a"(ret)
        : "a"(SYS_SOCKET), "D"(domain), "S"(type), "d"(protocol)
        : "memory"
    );
    return ret;
}

int bind(uint32_t sockfd, sockaddr_t* addr, uint32_t addrlen){
    int ret;
    asm volatile (
        "int $0x30"
        : "=a"(ret)
        : "a"(SYS_BIND), "D"(sockfd), "S"(addr), "d"(addrlen)
        : "memory"
    );
    return ret;
}

int recvfrom(uint32_t sockfd, void* buf, uint32_t len, uint32_t flags, sockaddr_t* src_addr, uint32_t addrlen){
    int ret;

    register uint64_t r10 asm("r10") = flags;
    register uint64_t r8  asm("r8")  = (uint64_t)src_addr;
    register uint64_t r9  asm("r9")  = addrlen;

    asm volatile (
        "int $0x30"
        : "=a"(ret)
        : "a"(SYS_RECVFROM), "D"(sockfd), "S"(buf), "d"(len), "r"(r10), "r"(r8), "r"(r9)
        : "memory"
    );
    return ret;
}

int sendto(uint32_t sockfd, void* buf, uint32_t len, uint32_t flags, sockaddr_t* dest_addr, uint32_t addrlen){
    int ret;
    register uint64_t r10 asm("r10") = flags;
    register uint64_t r8  asm("r8")  = (uint64_t)dest_addr;
    register uint64_t r9  asm("r9")  = addrlen;

    asm volatile (
        "int $0x30"
        : "=a"(ret)
        : "a"(SYS_SENDTO), "D"(sockfd), "S"(buf), "d"(len), "r"(r10), "r"(r8), "r"(r9)
        : "memory"
    );
    return ret;
}

int setsockopt(uint32_t sockfd, uint32_t level, uint32_t optname, void* optval, uint32_t optlen){
    int ret;
    register uint64_t r10 asm("r10") = (uint64_t)optval;
    register uint64_t r8  asm("r8")  = optlen;

    asm volatile (
        "int $0x30"
        : "=a"(ret)
        : "a"(SYS_SETSOCKOPT), "D"(sockfd), "S"(level), "d"(optname), "r"(r10), "r"(r8)
        : "memory"
    );
    return ret;    
}

int getsockopt(uint32_t sockfd, uint32_t level, uint32_t optname, void* optval, uint32_t* optlen){
    int ret;
    register uint64_t r10 asm("r10") = (uint64_t)optval;
    register uint64_t r8  asm("r8")  = (uint64_t)optlen;

    asm volatile (
        "int $0x30"
        : "=a"(ret)
        : "a"(SYS_GETSOCKOPT), "D"(sockfd), "S"(level), "d"(optname), "r"(r10), "r"(r8)
        : "memory"
    );
    return ret;    
}