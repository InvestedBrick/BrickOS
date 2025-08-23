
#ifndef INCLUDE_VFS_H
#define INCLUDE_VFS_H

typedef struct generic_file generic_file_t;
#include <stdint.h>
typedef struct
{
    generic_file_t* (*open)(unsigned char* filepath, uint8_t flags);
    int (*read)(generic_file_t* file,unsigned char* buffer, uint32_t size);  
    int (*write)(generic_file_t* file,unsigned char* buffer, uint32_t size);
    int (*close)(generic_file_t* file); 
} vfs_handlers_t;

typedef struct generic_file{
    vfs_handlers_t* ops;
    void* generic_data; // file specific data
} generic_file_t;
#endif