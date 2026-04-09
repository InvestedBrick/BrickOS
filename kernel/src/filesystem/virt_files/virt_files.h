#ifndef INCLUDE_VIRT_FILES_H
#define INCLUDE_VIRT_FILES_H

#include "../filesystem.h"
#include "../vfs/vfs.h"
#include "../../utilities/vector.h"
#include "virt_files_util.h"

typedef struct {
    uint32_t inode_id;
    generic_file_t* gen_file;
} virt_file_t;

extern vector_t virt_dirs;
extern vector_t virt_files;


/**
 * init_virt_dirs:
 * Initialises virtual directories like /sysinfo
*/
void init_virt_dirs();


/**
 * virt_file_open:
 * Opens a virtual file and returns a generic file to it
 * @param inode The inode of the virtual file
 * @return The generic file
 */
generic_file_t* virt_file_open(inode_t* inode);


#endif