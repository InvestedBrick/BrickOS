
#ifndef INCLUDE_DEVS_H
#define INCLUDE_DEVS_H

#include <stdint.h>
#include "../vfs/vfs.h"
#include "../filesystem.h"
#include "device_defines.h"
typedef struct {
    uint32_t inode_id;
    uint8_t priv_lvl;
    generic_file_t* gen_file;
}device_t;

extern framebuffer_t fb0;
void init_dev_vec();

/**
 * dev_open:
 * Opens a device and returns a generic file to it
 * @param inode The inode for the device file
 * 
 * @return A generic file to the device
 */
generic_file_t* dev_open(inode_t* inode);

/**
 * initialize_devices:
 * Initializes the devices
 */
void initialize_devices();
#endif