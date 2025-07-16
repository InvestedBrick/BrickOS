#include "syscalls.h"
#include "../filesystem/vfs/vfs.h"
#include "../processes/user_process.h"
#include "../filesystem/file_operations.h"

int sys_write(user_process_t* p,unsigned int fd, unsigned char* buf, unsigned int size){
    if (fd >= MAX_FDS) return SYSCALL_FAIL;

    generic_file_t* file = p->fd_table[fd];

    if (!file || !file->ops || !file->ops->write) return SYSCALL_FAIL;

    return file->ops->write(file,buf,size);
}
int sys_read(user_process_t* p,unsigned int fd, unsigned char* buf, unsigned int size){
    if (fd >= MAX_FDS) return SYSCALL_FAIL;

    generic_file_t* file = p->fd_table[fd];

    if (!file || !file->ops || !file->ops->read) return SYSCALL_FAIL;

    return file->ops->read(file,buf,size);
}

int sys_open(user_process_t* p,unsigned char* filepath, unsigned char flags){
    generic_file_t* file = fs_open(filepath,flags);

    if (!file) return SYSCALL_FAIL;

    return assign_fd(p,file);
}

int sys_close(user_process_t* p, unsigned int fd){
    if (fd >= MAX_FDS) return SYSCALL_FAIL;

    generic_file_t* file = p->fd_table[fd];

    if (!file || !file->ops || !file->ops->close) return SYSCALL_FAIL;

    free_fd(p,file);

    return file->ops->close(file);
}