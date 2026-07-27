
#ifndef INCLUDE_VFS_H
#define INCLUDE_VFS_H

typedef struct generic_file generic_file_t;
#include <stdint.h>

typedef enum {
    FILE_TYPE_REGULAR,
    FILE_TYPE_SOCKET,
    FILE_TYPE_PIPE,
    FILE_TYPE_DEVICE,
    FILE_TYPE_VIRT,
} file_type_e;

typedef struct{
    generic_file_t* (*open)(unsigned char* filepath, uint8_t flags);
    int (*read)(generic_file_t* file,unsigned char* buffer, uint32_t size);  
    int (*write)(generic_file_t* file,unsigned char* buffer, uint32_t size);
    int (*close)(generic_file_t* file); 
    int (*seek)(generic_file_t* file, uint32_t offset,uint32_t whence);
    int (*ioctl)(generic_file_t* file, uint32_t cmd, void* arg);
} vfs_handles_t;

typedef struct generic_file{
    vfs_handles_t* ops;
    void* generic_data; // file specific data
    file_type_e type; 
    uint32_t object_id;
} generic_file_t;
#endif