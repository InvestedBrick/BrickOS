#ifndef INCLUDE_VIRT_FILES_UTIL_H
#define INCLUDE_VIRT_FILES_UTIL_H

#include "../filesystem.h"
#include "../vfs/vfs.h"
#include "../../utilities/vector.h"

typedef struct {
    uint32_t inode_id;
    vector_t entry_names;
}virt_dir_t;


/*
 * get_all_names_in_virt_dir:
 * Gets all the names of the entries in a virtual directory
 * @param virt_dir The virtual directory
 * @return An array of strings containing the names
*/
string_array_t* get_all_names_in_virt_dir(inode_t* virt_dir);

/**
 * get_virt_dir_names_in_dir:
 * Gets the names of the entries in a virtual directory that is inside a physical directory
 * @param phys_dir The physical directory
 * @important The returned strings array needs to be freed by the caller
 * @return An array of strings containing the names if there a any
 */
string_array_t* get_virt_dir_names_in_dir(inode_t* phys_dir);


/**
 * create_virt_file:
 * Creates a virtual file
 * @param parent_dir The inode of the parent directory
 * @param name The name of the file
 * @param handlers The vfs handlers for read / write etc
 * @param priv_lvl The priviledge level needed to access this file
 */
void create_virt_file(inode_t* parent_dir,unsigned char* name,vfs_handles_t* handles,uint8_t priv_lvl);

/**
 * create_virt_dir:
 * Creates a virtual directory
 * @param parent_dir The inode of the parent directory
 * @param name The name of the virtual directory
 * @return The inode of the created virtual directory 
 */
inode_t* create_virt_dir(inode_t* parent_dir, unsigned char* name);
#endif