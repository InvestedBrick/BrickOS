
#ifndef INCLUDE_FS_H
#define INCLUDE_FS_H

#include "../../../kernel/src/filesystem/fs_defines.h"
#include <stdint.h>
typedef struct {
    uint32_t inode_id;
    uint32_t len;
    uint32_t type;
    unsigned char name[];
} dirent_t;

#endif