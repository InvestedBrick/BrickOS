#include "virt_files.h"
#include "../../memory/kmalloc.h"
#include "../../io/log.h"
#include "../../memory/memory.h"
vector_t virt_dirs;
vector_t virt_files;

generic_file_t* virt_file_open(inode_t* inode){
    for (uint32_t i = 0; i < virt_files.size;i++){
        virt_file_t* virt_file = (virt_file_t*)virt_files.data[i];
        if (virt_file->inode_id == inode->id){
            // create copy of generic file, since we want the original to be always available
            generic_file_t* file = (generic_file_t*)kmalloc(sizeof(generic_file_t));
            memcpy(file,virt_file->gen_file,sizeof(generic_file_t));
            return file;
        }
    }

    return nullptr;

}

int virt_file_generic_close(generic_file_t* file){
    return 0;
}


int virt_file_meminfo_read(generic_file_t* file,unsigned char* buffer,uint32_t size){
    if (size < 2 * 21 + 2) return -1; // 2 numbers with each max 21 digits + 2 terminators
    write_bufferf(buffer,size,"%d-%d",total_pages,mem_number_vpages);

    return strlen(buffer);
}

vfs_handles_t meminfo_handles = {
    .open = 0,
    .close = virt_file_generic_close,
    .read = virt_file_meminfo_read,
    .write = 0,
    .ioctl = 0,
    .seek = 0,
};

void init_virt_dirs(){
    init_vector(&virt_dirs);
    init_vector(&virt_files);

    inode_t* root = get_inode_by_id(FS_ROOT_DIR_ID);
    inode_t* sysinfo = create_virt_dir(root,"sysinfo");
    create_virt_file(sysinfo,"meminfo",&meminfo_handles,PRIV_STD);
}