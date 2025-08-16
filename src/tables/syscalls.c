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
#include "../filesystem/filesystem.h"
#include "../memory/kmalloc.h"

//TODO: provide actually helpful error messages
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

    int ret_val = file->ops->close(file);
    kfree(file);
    return ret_val;
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

int sys_getdents(user_process_t* p,unsigned int fd,dirent_t* ent_buffer,unsigned int size){
    if (!p->fd_table[fd]) return SYSCALL_FAIL;
    generic_file_t* file = p->fd_table[fd];

    if (!file->generic_data) return SYSCALL_FAIL;

    open_file_t* open_file = (open_file_t*)file->generic_data;

    inode_t* inode = get_inode_by_id(open_file->inode_id);
    string_array_t* names = get_all_names_in_dir(inode);

    if (!names) return SYSCALL_FAIL;

    unsigned int total_size = 0;
    for (unsigned int i = 0; i < names->n_strings;i++)
        {total_size += names->strings[i].length + sizeof(int) * 3 + 1;} // +1 for the null terminator
    if (total_size > size) return SYSCALL_FAIL; 

    unsigned int bpos = 0;
    for (unsigned int i = 0; i < names->n_strings;i++){
        inode_t* entry_inode = get_inode_by_id(get_inode_id_by_name(open_file->inode_id,names->strings[i].str));
        dirent_t* entry = (dirent_t*)((unsigned int)ent_buffer + bpos);
        entry->inode_id = entry_inode->id;
        entry->type = entry_inode->type;
        entry->len = sizeof(int) * 3 + names->strings[i].length + 1;
        memcpy(entry->name,names->strings[i].str,names->strings[i].length + 1);
        bpos += entry->len;
    }
    free_string_arr(names);
    return names->n_strings;
}

int sys_chdir(unsigned char* dir_name){
    inode_t* new_dir;

    if (dir_contains_name(active_dir,dir_name) || strneq(dir_name,"..",2,2)){
        new_dir = get_inode_by_relative_file_path(dir_name);
    }else{
        new_dir = get_inode_by_full_file_path(dir_name);
    }
    if (!new_dir) return SYSCALL_FAIL;
    if (new_dir->type != FS_TYPE_DIR) return SYSCALL_FAIL;

    active_dir = new_dir;

    return SYSCALL_SUCCESS;
}

int sys_rmfile(unsigned char* filename){
    inode_t* file;

    if (dir_contains_name(active_dir,filename)){
        file = get_inode_by_relative_file_path(filename);
    }else{
        file = get_inode_by_full_file_path(filename);
    }
    if (!file) return SYSCALL_FAIL;

    inode_t* parent_dir = get_parent_inode(file);

    if (delete_file_by_inode(parent_dir,file) < 0) return SYSCALL_FAIL;

    return SYSCALL_SUCCESS;

}