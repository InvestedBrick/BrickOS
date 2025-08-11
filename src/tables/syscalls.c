#include "syscalls.h"
#include "syscall_defines.h"
#include "../filesystem/vfs/vfs.h"
#include "../processes/user_process.h"
#include "../filesystem/file_operations.h"
#include "interrupts.h"
#include "../memory/memory.h"
#include "../io/log.h"
#include "../processes/scheduler.h"
#include "../util.h"
#include "../filesystem/fsutil.h"
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

int sys_exit(user_process_t* p,interrupt_stack_frame_t* stack_frame){
    unsigned int pid = p->process_id;
    log("Process exited with error code");
    log_uint(stack_frame->ebx);
    switch_task(stack_frame);
    return kill_user_process(pid);
}

int sys_mmap(user_process_t* p,unsigned int size){
    
    unsigned int pages_to_alloc = CEIL_DIV(size,MEMORY_PAGE_SIZE);
    for (unsigned int i = 0; i < pages_to_alloc; i++){
        //ensure continuous allocation
        unsigned int page = pmm_alloc_page_frame();
        mem_map_page(p->page_alloc_start,page,PAGE_FLAG_WRITE | PAGE_FLAG_USER);
        
        p->page_alloc_start += MEMORY_PAGE_SIZE;
    }
    
    return ((int)p->page_alloc_start  - (MEMORY_PAGE_SIZE * pages_to_alloc)); // int conversion (kinda) safe because surely a single process wont allocate 2gb of RAM when we only hav 512 MB
}

int sys_getcwd(unsigned char* buffer, unsigned int buf_len){
    return get_full_active_path(buffer,buf_len);
}