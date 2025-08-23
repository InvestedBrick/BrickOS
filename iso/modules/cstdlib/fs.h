
#ifndef INCLUDE_FS_H
#define INCLUDE_FS_H

#include "../../../src/filesystem/fs_defines.h"
#include <stdint.h>
typedef struct {
    uint32_t inode_id;
    uint32_t len;
    uint32_t type;
    uint8_t name[];
} dirent_t;

#endif