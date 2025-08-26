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
#include <stdbool.h>

//TODO: provide actually helpful error messages
int sys_write(user_process_t* p,uint32_t fd, unsigned char* buf, uint32_t size){
    if (fd >= MAX_FDS) return SYSCALL_FAIL;

    generic_file_t* file = p->fd_table[fd];

    if (!file || !file->ops || !file->ops->write) return SYSCALL_FAIL;

    return file->ops->write(file,buf,size);
}
int sys_read(user_process_t* p,uint32_t fd, unsigned char* buf, uint32_t size){
    if (fd >= MAX_FDS) return SYSCALL_FAIL;

    generic_file_t* file = p->fd_table[fd];

    if (!file || !file->ops || !file->ops->read) return SYSCALL_FAIL;

    return file->ops->read(file,buf,size);
}

int sys_open(user_process_t* p,unsigned char* filepath, uint8_t flags){

    generic_file_t* file = fs_open(filepath,flags);

    if (!file) return SYSCALL_FAIL;

    return assign_fd(p,file);
}

int sys_close(user_process_t* p, uint32_t fd){
    if (fd >= MAX_FDS) return SYSCALL_FAIL;

    generic_file_t* file = p->fd_table[fd];

    if (!file || !file->ops || !file->ops->close) return SYSCALL_FAIL;

    free_fd(p,file);

    int ret_val = file->ops->close(file);
    kfree(file);
    return ret_val;
}

int sys_seek(user_process_t* p,uint32_t fd, uint32_t offset){
    if (fd >= MAX_FDS) return SYSCALL_FAIL;

    generic_file_t* file = p->fd_table[fd];

    if (!file || !file->ops || !file->ops->seek) return SYSCALL_FAIL;

    return file->ops->seek(file,offset);
}

int sys_ioctl(user_process_t* p, uint32_t fd,uint32_t cmd, void* arg){
    if (fd >= MAX_FDS) return SYSCALL_FAIL;

    generic_file_t* file = p->fd_table[fd];

    if (!file || !file->ops || !file->ops->ioctl) return SYSCALL_FAIL;

    return file->ops->ioctl(file,cmd,arg);
}

int sys_exit(user_process_t* p,interrupt_stack_frame_t* stack_frame){
    uint32_t pid = p->process_id;
    log("Process exited with error code");
    log_uint(stack_frame->ebx);
    switch_task(stack_frame);
    return kill_user_process(pid);
}

int sys_mmap(user_process_t *p, void *addr, uint32_t size,uint32_t prot, uint32_t flags, uint32_t fd, uint32_t offset){
    if (size == 0 || (offset % MEMORY_PAGE_SIZE) != 0) return SYSCALL_FAIL;
    
    if (fd < 3) fd = MAP_FD_NONE;

    if (flags & MAP_ANONYMOUS && flags & MAP_SHARED) return SYSCALL_FAIL; // not implemented yet

    if (!(flags & MAP_ANONYMOUS)){
        // we are mapping a file
        if (!p->fd_table[fd]) return SYSCALL_FAIL;
    }

    if (!addr) addr = (void*)p->page_alloc_start;

    uint32_t n_pages = CEIL_DIV(size,MEMORY_PAGE_SIZE);
    size = n_pages * MEMORY_PAGE_SIZE;

    virt_mem_area_t* vma = (virt_mem_area_t*)kmalloc(sizeof(virt_mem_area_t));

    vma->addr = addr;
    vma->size = size;
    vma->fd = fd;
    vma->flags = flags;
    vma->prot = prot;
    vma->offset = offset;
    
    if (flags & MAP_SHARED){
        shared_object_t* shrd_obj = find_shared_object_by_id(p->fd_table[fd]->object_id);

        if (!shrd_obj){
            shrd_obj = (shared_object_t*)kmalloc(sizeof(shared_object_t));
            shrd_obj->n_pages = n_pages;
            shrd_obj->ref_count = 1;
            shrd_obj->unique_id = p->fd_table[fd]->object_id;
            shrd_obj->shared_pages = (shared_page_t**)kmalloc(n_pages * sizeof(shared_page_t*));
            memset(shrd_obj->shared_pages,0x0,n_pages * sizeof(shared_page_t*));
            append_shared_object(shrd_obj);
        }else{
            shrd_obj->ref_count++;
        }
    
        if (n_pages > shrd_obj->n_pages){

            uint32_t old_n_pages = shrd_obj->n_pages;
            shrd_obj->shared_pages = (shared_page_t**)
                                        realloc(shrd_obj->shared_pages,
                                            shrd_obj->n_pages * sizeof(shared_page_t*),
                                            n_pages * sizeof(shared_page_t*)
                                     );
            // set the new shared pages null
            memset((void*)&shrd_obj->shared_pages[old_n_pages],0x0,(shrd_obj->n_pages - old_n_pages) * sizeof(shared_page_t*));
        }

        vma->shrd_obj = shrd_obj;
    }else{
        vma->shrd_obj = nullptr;
    }
    
    vma->next = p->vm_areas;
    p->vm_areas = vma;

    p->page_alloc_start = (uint32_t)addr + size;

    // we kinda just pretend that the area is mapped and map it once we fault there
    return (int)addr;
}

int sys_getcwd(unsigned char* buffer, uint32_t buf_len){
    return get_full_active_path(buffer,buf_len);
}

int sys_getdents(user_process_t* p,uint32_t fd,dirent_t* ent_buffer,uint32_t size){
    if (!p->fd_table[fd]) return SYSCALL_FAIL;
    generic_file_t* file = p->fd_table[fd];

    if (!file->generic_data) return SYSCALL_FAIL;

    open_file_t* open_file = (open_file_t*)file->generic_data;

    inode_t* inode = get_inode_by_id(open_file->inode_id);

    if (!inode) return nullptr;

    string_array_t* names = get_all_names_in_dir(inode);

    uint32_t total_size = 0;
    for (uint32_t i = 0; i < names->n_strings;i++)
        {total_size += names->strings[i].length + sizeof(int) * 3 + 1;} // +1 for the null terminator
    if (total_size > size) return SYSCALL_FAIL; 

    uint32_t bpos = 0;
    for (uint32_t i = 0; i < names->n_strings;i++){
        inode_t* entry_inode = get_inode_by_id(get_inode_id_by_name(open_file->inode_id,names->strings[i].str));
        dirent_t* entry = (dirent_t*)((uint32_t)ent_buffer + bpos);
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
    inode_t* new_dir = get_inode_by_path(dir_name);
    if (!new_dir) return SYSCALL_FAIL;
    if (new_dir->type != FS_TYPE_DIR) return SYSCALL_FAIL;

    active_dir = new_dir;

    return SYSCALL_SUCCESS;
}

int sys_rmfile(unsigned char* filename){
    inode_t* file = get_inode_by_path(filename);

    if (!file) return SYSCALL_FAIL;

    inode_t* parent_dir = get_parent_inode(file);

    if (delete_file_by_inode(parent_dir,file) < 0) return SYSCALL_FAIL;

    return SYSCALL_SUCCESS;

}

int sys_mknod(unsigned char* filename,uint32_t type){
    if (get_inode_by_path(filename)) return SYSCALL_FAIL;

    if (type != FS_TYPE_PIPE) return SYSCALL_FAIL; // add other types later


    inode_t* creation_dir = active_dir;

    string_array_t* str_arr = split_filepath(filename);

    unsigned char* file_name;
    if (str_arr->n_strings > 1){
        creation_dir = get_inode_by_path(str_arr->strings[0].str);

        if (!creation_dir){
            free_string_arr(str_arr);
            return SYSCALL_FAIL;
        }

        file_name = str_arr->strings[1].str;
    }else{
        file_name = str_arr->strings[0].str;
    }

    if (create_file(creation_dir,file_name,strlen(file_name),type,FS_FILE_PERM_NONE) < 0) {
        free_string_arr(str_arr);
        return SYSCALL_FAIL;
    }

    free_string_arr(str_arr);

    return SYSCALL_SUCCESS;
}