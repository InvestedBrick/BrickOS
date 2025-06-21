#include "file_operations.h"
#include "filesystem.h"
#include "../util.h"
#include "../memory/kmalloc.h"

vector_t fd_vector;
static unsigned char fd_used[MAX_FDS] = {0};
static unsigned int next_fd = 1;

unsigned int get_fd(){
    for (unsigned int i = 0; i < MAX_FDS; ++i) {
        unsigned int fd = next_fd;
        next_fd = (next_fd % MAX_FDS) + 1;
        if (!fd_used[fd]) {
            fd_used[fd] = 1;
            return fd;
        }
    }
    return 0;
}

unsigned int free_fd(unsigned int fd){
    if (fd > 0 && fd < MAX_FDS){
        fd_used[fd] = 0;
    }
}

int open(unsigned char* filepath,unsigned char flags){
    inode_t* inode = get_inode_by_full_file_path(filepath);
    if (!inode) return FILE_OP_FAILED;

    open_file_t* file = (open_file_t*)kmalloc(sizeof(open_file_t));
    file->fd = get_fd();
    if (!file->fd) return FILE_OP_FAILED;
    file->inode_id = inode->id;
    file->flags = flags;

    vector_append(&fd_vector,(unsigned int)file);
    return file->fd;
}

int close(unsigned int fd){
    for(unsigned int i = 0; i < fd_vector.size;++i){
        open_file_t* file = (open_file_t*)fd_vector.data[i];
        if (file->fd == fd){
            free_fd(fd);
            vector_erase(&fd_vector,i);
            kfree(file);
            return FILE_OP_SUCCESS;
        }
    }

    return FILE_INVALID_FD;
}

int write(unsigned int fd, unsigned char* buffer,unsigned int size){
    //TODO
}



int read(unsigned int fd, unsigned char* buffer, unsigned int size){
    //TODO
}