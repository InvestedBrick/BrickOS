#include "devs.h"
#include "../vfs/vfs.h"
#include "../filesystem.h"
#include "../../drivers/ATA_PIO/ata.h"
#include "../../vector.h"
#include "../../tables/syscalls.h"
#include "../../kernel_header.h"
#include "../../io/log.h"
#include "../../memory/kmalloc.h"
#include <stdint.h>

vector_t dev_vec;
uint32_t unique_id = (FS_ROOT_DIR_ID + 1);
void init_dev_vec(){
    init_vector(&dev_vec);
}

generic_file_t* dev_open(inode_t* inode){
    for (uint32_t i = 0; i < dev_vec.size;i++){
        device_t* dev = (device_t*)dev_vec.data[i];
        if (dev->inode_id == inode->id){
            return dev->gen_file;
        }
    }

    return 0;
}

void create_device(unsigned char* name,vfs_handles_t* handles, void* gen_data){
    
    inode_t* dev_dir = get_inode_by_full_file_path("dev/");
    if (!dev_dir) {error("/dev was not initialized"); return;}

    inode_t* old_dir = change_active_dir(dev_dir);

    if (!dir_contains_name(active_dir,name)){
        if (create_file(active_dir,name,strlen(name),FS_TYPE_DEV,FS_FILE_PERM_NONE) < 0)
        error("Failed to create device");
    }
    
    inode_t* node = get_inode_by_relative_file_path(name);

    device_t* dev = (device_t*)kmalloc(sizeof(device_t));

    dev->inode_id = node->id;
    dev->gen_file = (generic_file_t*)kmalloc(sizeof(generic_file_t));

    dev->gen_file->generic_data = gen_data;
    dev->gen_file->ops = handles;
    dev->gen_file->object_id = unique_id++;

    vector_append(&dev_vec,(uint32_t)&dev);

    change_active_dir(old_dir);
}

int dev_null_read(generic_file_t* file,unsigned char* buffer,uint32_t size){
    memset(buffer,0,size);
    return size;
}
int dev_null_write(generic_file_t* file,unsigned char* buffer, uint32_t size){
    return size;
}
int dev_null_close(generic_file_t* file){
    return 0;
}

vfs_handles_t dev_null = {
    .open = 0,
    .close = dev_null_close,
    .read = dev_null_read,
    .write = dev_null_write,
};

void initialize_devices(){
    create_device("null",&dev_null,0);

}
