
#ifndef INCLUDE_PIPES_H
#define INCLUDE_PIPES_H
#include <stdint.h>
#include "../vfs/vfs.h"
#include "../filesystem.h"
#define PIPE_DEFAULT_ACCESS_ID (uint32_t)-1

typedef struct {
    unsigned char* buffer;
    uint32_t buffer_size;
    uint32_t inode_id;

    uint32_t read_tail;
    uint32_t write_head;

    uint32_t read_id;
    uint32_t write_id;

    uint8_t closed;
}pipe_t;

void init_pipe_vec();

/**
 * open_pipe:
 * Creates a pipe_t struct and allocates memory for the pipe
 * 
 * @param inode The inode for the pipe file
 * @param flags The flags (R/W) for opening the pipe
 * 
 * @return A generic file for the pipe
 */
generic_file_t* open_pipe(inode_t* inode,uint32_t flags);
#endif