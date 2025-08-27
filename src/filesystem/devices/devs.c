#include "devs.h"
#include "../vfs/vfs.h"
#include "../filesystem.h"
#include "../../drivers/ATA_PIO/ata.h"
#include "../../vector.h"
#include "../../tables/syscalls.h"
#include "../../kernel_header.h"
#include "../../io/log.h"
#include "../../memory/kmalloc.h"
#include "../../memory/memory.h"
#include "../../processes/user_process.h"
#include "device_defines.h"
#include <stdint.h>

vector_t dev_vec;

void init_dev_vec(){
    init_vector(&dev_vec);
}

void init_dev_fb0(device_t* dev){
    shared_object_t* shrd_obj = (shared_object_t*)kmalloc(sizeof(shared_object_t));
    framebuffer_t* fb = (framebuffer_t*)dev->gen_file->generic_data;

    uint32_t n_pages = CEIL_DIV(fb->size, MEMORY_PAGE_SIZE);
    shrd_obj->ref_count = 1;

    shrd_obj->n_pages = n_pages;
    shrd_obj->unique_id = dev->gen_file->object_id;
    shrd_obj->shared_pages = (shared_page_t**)kmalloc(n_pages * sizeof(shared_page_t*));
    
    for (uint32_t i = 0; i < n_pages;i++){
        shrd_obj->shared_pages[i] = (shared_page_t*)kmalloc(sizeof(shared_page_t));
        shrd_obj->shared_pages[i]->ref_count = 1;
        shrd_obj->shared_pages[i]->phys_addr = fb->phys_addr + i * MEMORY_PAGE_SIZE;
    } 

    append_shared_object(shrd_obj);
}

generic_file_t* dev_open(inode_t* inode){
    user_process_t* current_proc = get_current_user_process();

    for (uint32_t i = 0; i < dev_vec.size;i++){
        device_t* dev = (device_t*)dev_vec.data[i];
        if (dev->inode_id == inode->id){
            return dev->gen_file;
        }
    }

    return nullptr;
}

void create_device(unsigned char* name,vfs_handles_t* handles, void* gen_data,void (*init)(device_t*),uint8_t priv_lvl){
    
    inode_t* dev_dir = get_inode_by_full_file_path("dev/");
    if (!dev_dir) {error("/dev was not initialized"); return;}

    inode_t* old_dir = change_active_dir(dev_dir);

    if (!dir_contains_name(active_dir,name)){
        if (create_file(active_dir,name,strlen(name),FS_TYPE_DEV,FS_FILE_PERM_NONE,priv_lvl) < 0)
        error("Failed to create device");
    }
    
    inode_t* node = get_inode_by_relative_file_path(name);

    device_t* dev = (device_t*)kmalloc(sizeof(device_t));

    dev->inode_id = node->id;
    dev->gen_file = (generic_file_t*)kmalloc(sizeof(generic_file_t));
    dev->gen_file->generic_data = gen_data;
    dev->gen_file->ops = handles;
    dev->gen_file->object_id = node->id;

    if (init){
        init(dev);
    }
    
    vector_append(&dev_vec,(uint32_t)dev);
    change_active_dir(old_dir);
}

int dev_null_read(generic_file_t* file,unsigned char* buffer,uint32_t size){
    memset(buffer,0,size);
    return size;
}
int dev_null_write(generic_file_t* file,unsigned char* buffer, uint32_t size){
    return size;
}
int dev_generic_close(generic_file_t* file){
    return 0;
}

vfs_handles_t dev_null = {
    .open = 0,
    .close = dev_generic_close,
    .read = dev_null_read,
    .write = dev_null_write,
    .seek = 0,
    .ioctl = 0,
};

framebuffer_t fb0;

int dev_fb0_ioctl(generic_file_t* file, uint32_t cmd,void* arg){
    if (!arg) return -1;  
    framebuffer_t* fb0 = (framebuffer_t*)file->generic_data;
    switch (cmd)
    {
    case DEV_FB0_GET_METADATA: {
            memcpy(arg,(void*)fb0,sizeof(framebuffer_t));
            (*(framebuffer_t*)arg).phys_addr = 0; // not telling you that
            break;
        }
    
    default:
        return -1;
    }

    return 0;
}

vfs_handles_t dev_fb0 = {
    .open = 0,
    .close = dev_generic_close,
    .read = 0,
    .write = 0, // we only write to a mapped fb
    .seek = 0,
    .ioctl = dev_fb0_ioctl,
};

wm_pipes_t wm_pipes;

void init_dev_wm(device_t* dev){

    wm_pipes_t* pipes = (wm_pipes_t*)dev->gen_file->generic_data;
    // set up two pipes for wm <-> kernel communication
    if (sys_mknod("tmp/wm_to_k.tmp",TYPE_PIPE) < 0) error("Failed to create tmp/wm_to_k.tmp");
    if (sys_mknod("tmp/k_to_wm.tmp",TYPE_PIPE) < 0) error("Failed to create tmp/k_to_wm.tmp");

    change_file_priviledge_level("tmp/wm_to_k.tmp",PRIV_SPECIAL);
    change_file_priviledge_level("tmp/k_to_wm.tmp",PRIV_SPECIAL);

    pipes->wm_to_k_fd = sys_open(&global_kernel_process,"tmp/wm_to_k.tmp",FILE_FLAG_READ);
    pipes->k_to_wm_fd = sys_open(&global_kernel_process,"tmp/k_to_wm.tmp",FILE_FLAG_WRITE);

    if (pipes->wm_to_k_fd < 0) error("Failed to open tmp/wm_to_k.tmp");
    if (pipes->k_to_wm_fd < 0) error("Failed to open tmp/k_to_wm.tmp");

    sys_write(&global_kernel_process,pipes->k_to_wm_fd,"\nHello from kernel\n",sizeof("\nHello from kernel\n"));
}

int dev_wm_ioctl(generic_file_t* file, uint32_t cmd,void* arg){
    // user process <-> window manager
}

vfs_handles_t dev_wm = {
    .open = 0,
    .close = dev_generic_close,
    .read = 0,
    .write = 0,
    .seek = 0,
    .ioctl = dev_wm_ioctl,
};

void initialize_devices(){
    create_device("null",&dev_null,0,0,PRIV_STD);
    create_device("fb0",&dev_fb0,&fb0,init_dev_fb0,PRIV_SPECIAL);
    create_device("wm",&dev_wm,&wm_pipes,init_dev_wm,PRIV_STD);
}
