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
#include "../../drivers/keyboard/keyboard.h"
#include "device_defines.h"
#include <stdint.h>

vector_t dev_vec;
vector_t wm_answers;
wm_pipes_t wm_pipes;
framebuffer_t fb0;

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
    for (uint32_t i = 0; i < dev_vec.size;i++){
        device_t* dev = (device_t*)dev_vec.data[i];
        if (dev->inode_id == inode->id){
            // create copy of generic file, since we want the original to be always available
            generic_file_t* file = (generic_file_t*)kmalloc(sizeof(generic_file_t));
            memcpy(file,dev->gen_file,sizeof(generic_file_t));
            return file;
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


void init_dev_wm(device_t* dev){

    init_vector(&wm_answers);
    wm_pipes_t* pipes = (wm_pipes_t*)dev->gen_file->generic_data;
    // set up two pipes for wm <-> kernel communication
    mknod_params_t params = {
        .type = TYPE_PIPE,
        .flags = 0,
    };
    if (sys_mknod("tmp/wm_to_k.tmp",&params) < 0) error("Failed to create tmp/wm_to_k.tmp");
    if (sys_mknod("tmp/k_to_wm.tmp",&params) < 0) error("Failed to create tmp/k_to_wm.tmp");

    change_file_priviledge_level("tmp/wm_to_k.tmp",PRIV_SPECIAL);
    change_file_priviledge_level("tmp/k_to_wm.tmp",PRIV_SPECIAL);

    pipes->wm_to_k_fd = sys_open(&global_kernel_process,"tmp/wm_to_k.tmp",FILE_FLAG_READ);
    pipes->k_to_wm_fd = sys_open(&global_kernel_process,"tmp/k_to_wm.tmp",FILE_FLAG_WRITE);

    if (pipes->wm_to_k_fd < 0) error("Failed to open tmp/wm_to_k.tmp");
    if (pipes->k_to_wm_fd < 0) error("Failed to open tmp/k_to_wm.tmp");

}

void dev_wm_collect_pipe(){
    unsigned char* buffer = (unsigned char*)kmalloc(MEMORY_PAGE_SIZE);
    overwrite_current_proc(&global_kernel_process);
    int n_read = sys_read(&global_kernel_process,wm_pipes.wm_to_k_fd,buffer,MEMORY_PAGE_SIZE);
    restore_active_proc();
    if (n_read <= 0) {
        kfree(buffer); 
        return;
    }
    uint32_t bytes_parsed = 0;
    while (bytes_parsed < n_read){
        uint32_t answer_type = *(uint32_t*)&buffer[bytes_parsed];
        
        switch (answer_type)
        {
        case DEV_WM_ANS_TYPE_WIN_CREATION: {

            window_creation_ans_t* answer = (window_creation_ans_t*)kmalloc(sizeof(window_creation_ans_t));
            memcpy(answer,&buffer[bytes_parsed],sizeof(window_creation_ans_t));
            // make space for the following name
            answer = (window_creation_ans_t*)realloc(answer,sizeof(window_creation_ans_t),sizeof(window_creation_ans_t) + answer->filename_len);
            bytes_parsed += sizeof(window_creation_ans_t);  // the pointer is not actually passed, just part of the struct
            
            memcpy(answer->filename,&buffer[bytes_parsed],answer->filename_len);
            bytes_parsed += answer->filename_len;

            vector_append(&wm_answers,(uint32_t)answer);
            break;
        }        
        default:
            break;
        }
    }
}

int dev_wm_ioctl(generic_file_t* file, uint32_t cmd,void* arg){
    user_process_t* curr_proc = get_current_user_process();
    typedef struct {
        uint32_t answer_type;
        uint32_t pid;
    }answer_header_t;

    // user process <-[kernel]-> window manager
    switch (cmd)
    {
    case DEV_WM_REQUEST_WINDOW: {
        window_req_t* data = (window_req_t*)arg;
        uint32_t cmd_hdr[2] = {DEV_WM_REQUEST_WINDOW,curr_proc->process_id};
        overwrite_current_proc(&global_kernel_process);
        sys_write(&global_kernel_process,wm_pipes.k_to_wm_fd,(unsigned char*)cmd_hdr,sizeof(cmd_hdr));
        sys_write(&global_kernel_process,wm_pipes.k_to_wm_fd,(unsigned char*)data,sizeof(window_req_t));
        restore_active_proc();
        return 0;
    }
    case DEV_WM_REQUEST_WINDOW_CREATION_ANSWER: {
        dev_wm_collect_pipe();
        for (uint32_t i = 0; i < wm_answers.size;i++){
            answer_header_t* hdr = (answer_header_t*)wm_answers.data[i];

            if (hdr->answer_type == DEV_WM_ANS_TYPE_WIN_CREATION && hdr->pid == curr_proc->process_id){
                // got some mail for you!
                window_creation_wm_answer_t* answer = (window_creation_wm_answer_t*)arg;
                window_creation_ans_t* data = (window_creation_ans_t*)wm_answers.data[i]; // I really need to rename these structs
                answer->height = data->height;
                answer->width = data->width;
                answer->kb_fd = data->kb_fd;
                answer->filename_len = data->filename_len;
                memcpy(answer->filename,data->filename,data->filename_len);

                vector_erase(&wm_answers,i);

                return 0;
            
            }
        }

        break;
    }
    case DEV_WM_COMMIT_WINDOW: {

        uint32_t cmd_hdr[2] = {DEV_WM_COMMIT_WINDOW,curr_proc->process_id};
        overwrite_current_proc(&global_kernel_process);
        sys_write(&global_kernel_process,wm_pipes.k_to_wm_fd,(unsigned char*)cmd_hdr,sizeof(cmd_hdr));
        restore_active_proc();
        return 0;
    }
    default:
        break;
    }

    return -1;
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
    create_device("kb0",&kb_ops,0,0,PRIV_SPECIAL);
}
