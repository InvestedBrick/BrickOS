
#ifndef INCLUDE_FS_H
#define INCLUDE_FS_H

#include "../../../src/filesystem/fs_defines.h"

typedef struct {
    unsigned int inode_id;
    unsigned int len;
    unsigned int type;
    unsigned char name[];
} dirent_t;

#endif