
#ifndef INCLUDE_VFS_H
#define INCLUDE_VFS_H

typedef struct generic_file generic_file_t;

typedef struct
{
    generic_file_t* (*open)(unsigned char* filepath, unsigned char flags);
    int (*read)(generic_file_t* file,unsigned char* buffer, unsigned int size);  
    int (*write)(generic_file_t* file,unsigned char* buffer, unsigned int size);
    int (*close)(generic_file_t* file); 
} vfs_handlers_t;

typedef struct generic_file{
    vfs_handlers_t* ops;
    void* generic_data; // file specific data
} generic_file_t;
#endif