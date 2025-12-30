#include "pipes.h"
#include "../../utilities/util.h"
#include "../../utilities/vector.h"
#include "../../processes/user_process.h"
#include "../filesystem.h"
#include "../fs_defines.h"
#include "../../memory/kmalloc.h"
#include "../../memory/memory.h"
#include "../vfs/vfs.h"
#include "../../io/log.h"
#include "../../kernel_header.h"
#include <stdint.h>
#include <stdbool.h>
vector_t pipe_vec;

pipe_t* find_pipe_by_inode(inode_t* inode){
    for (uint32_t i = 0; i < pipe_vec.size;i++){
        pipe_t* pipe = (pipe_t*)pipe_vec.data[i];
        if (pipe->inode_id == inode->id) return pipe;
    }

    return nullptr;
}

void init_pipe_vec(){
    init_vector(&pipe_vec);
}

int pipe_read(generic_file_t* file, unsigned char* buffer,uint32_t size){
    pipe_t* pipe = (pipe_t*)file->generic_data;

    if (pipe->closed) return -1;
    
    user_process_t* proc = get_current_user_process();

    if (pipe->read_id != proc->process_id) return -1;

    uint32_t bytes_read = 0;

    while (bytes_read < size){
        if (pipe->read_tail == pipe->write_head) break;
        
        uint32_t bytes_until_end = (pipe->read_tail > pipe->write_head) ? (pipe->buffer_size - pipe->read_tail) : (pipe->write_head - pipe->read_tail);
        uint32_t remaining_size = size - bytes_read;
        uint32_t to_read = (bytes_until_end > remaining_size) ? remaining_size : bytes_until_end;

        memcpy(&buffer[bytes_read],&pipe->buffer[pipe->read_tail],to_read);
        bytes_read += to_read;
        pipe->read_tail = (pipe->read_tail + to_read) % pipe->buffer_size;
    }

    return bytes_read;
}
int pipe_write(generic_file_t* file, unsigned char* buffer, uint32_t size){
    pipe_t* pipe = (pipe_t*)file->generic_data;

    if (pipe->closed) return -1;

    user_process_t* proc = get_current_user_process();

    if (pipe->write_id != proc->process_id) return -1;

    uint32_t bytes_written = 0;

    while (bytes_written < size){
        uint32_t next_head = (pipe->write_head + 1) % pipe->buffer_size;
        if (next_head == pipe->read_tail) break;  // buffer is full

        uint32_t bytes_until_end = (pipe->write_head >= pipe->read_tail) ? (pipe->buffer_size - pipe->write_head) : (pipe->read_tail - pipe->write_head - 1);
        uint32_t remaining_size = size - bytes_written;
        uint32_t to_write = (bytes_until_end > remaining_size) ? remaining_size : bytes_until_end;

        memcpy(&pipe->buffer[pipe->write_head],&buffer[bytes_written],to_write);
        bytes_written += to_write;
        pipe->write_head = (pipe->write_head + to_write) % pipe->buffer_size;
    }

    return bytes_written;
}

int pipe_close(generic_file_t* file){
    pipe_t* pipe = (pipe_t*)file->generic_data;

    if (pipe->closed){
        // now both ends have closed the pipe
        inode_t* inode = get_inode_by_id(pipe->inode_id);
        
        if (inode){
            // inode might have already been deleted by a process
            inode_t* parent = get_parent_inode(inode);
            
            delete_file_by_inode(parent,inode);
        }
        
        kfree(pipe->buffer);

        vector_erase_item(&pipe_vec,(uint64_t)pipe);

        kfree(pipe);

    }else { pipe->closed = true; }

    return 0;

}

vfs_handles_t pipe_handlers = {
    .open = 0,
    .read = pipe_read,
    .write = pipe_write,
    .close = pipe_close,
    .seek = 0,
    .ioctl = 0,
};

generic_file_t* open_pipe(inode_t* inode,uint32_t flags){
    pipe_t* pipe = find_pipe_by_inode(inode);
    
    if (!pipe){
        pipe = (pipe_t*)kmalloc(sizeof(pipe_t));
        pipe->inode_id = inode->id;
        pipe->write_head = 0;
        pipe->read_tail = 0;
        pipe->buffer_size = MEMORY_PAGE_SIZE;
        pipe->buffer = (unsigned char*)kmalloc(pipe->buffer_size);
        pipe->read_id = PIPE_DEFAULT_ACCESS_ID;
        pipe->write_id = PIPE_DEFAULT_ACCESS_ID;
        pipe->closed = false;
        vector_append(&pipe_vec,(vector_data_t)pipe);
        
    }

    if (flags & FILE_FLAG_READ && flags & FILE_FLAG_WRITE) return nullptr;

    user_process_t* proc = get_current_user_process();

    if (flags & FILE_FLAG_READ) {
        if (pipe->read_id != PIPE_DEFAULT_ACCESS_ID) return nullptr;

        pipe->read_id = proc->process_id;
    } else if (flags & FILE_FLAG_WRITE){
        if (pipe->write_id != PIPE_DEFAULT_ACCESS_ID) return nullptr;

        pipe->write_id = proc->process_id;
    }

    generic_file_t* gen_file = (generic_file_t*)kmalloc(sizeof(generic_file_t));
    gen_file->generic_data = (void*)pipe;
    gen_file->ops = &pipe_handlers;
    gen_file->object_id = inode->id;

    return gen_file;

}
