
#ifndef INCLUDE_FS_H
#define INCLUDE_FS_H

#define FILE_FLAG_NONE 0x0
#define FILE_FLAG_READ 0x1
#define FILE_FLAG_WRITE 0x2
#define FILE_PERM_EXEC 0x4
#define FILE_FLAG_CREATE 0x4
#define TYPE_FILE 1
#define TYPE_DIR 2

typedef struct {
    unsigned int inode_id;
    unsigned int len;
    unsigned int type;
    unsigned char name[];
} dirent_t;

#endif