#include "virt_files.h"
#include "../../memory/kmalloc.h"
#include "../../io/log.h"
#include "../../memory/memory.h"
#include "../../utilities/misc_instruc_wrappers.h"
#include "../../tables/interrupts.h"
#include "../../kernel_header.h"
#include "../../tables/syscalls.h"
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
    if (size < 2 * MAX_BYTES_64_BIT_INT + 2) return -1; // 2 numbers with each max 21 digits + 2 terminators
    write_bufferf(buffer,size,"%d-%d",total_pages,mem_number_vpages);

    return strlen(buffer);
}

int virt_file_cpuinfo_read(generic_file_t* file, unsigned char* buffer, uint32_t size){
    
    uint32_t regs[12];
    if (size < sizeof(regs) + 1 ) return 0;    

    _cpuid(CPUID_HIGHEST_EXT_FUNC_IMPLEMENTED,0,&regs[0],&regs[1],&regs[2],&regs[3]);
    if (regs[0] < CPUID_PROCESSOR_BRAND_STRING_2) return -1;

    _cpuid(CPUID_PROCESSOR_BRAND_STRING_0,0,&regs[0],&regs[1],&regs[2],&regs[3]);
    _cpuid(CPUID_PROCESSOR_BRAND_STRING_0,0,&regs[4],&regs[5],&regs[6],&regs[7]);
    _cpuid(CPUID_PROCESSOR_BRAND_STRING_0,0,&regs[8],&regs[9],&regs[10],&regs[11]);

    memcpy(buffer,regs,sizeof(regs));
    buffer[sizeof(regs)] = '\0';
    return sizeof(regs);
}

int virt_file_uptime_read(generic_file_t* file, unsigned char* buffer, uint32_t size){
    if (size < MAX_BYTES_64_BIT_INT + 1) return -1;
    write_bufferf(buffer,size,"%d",current_timestamp - limine_data.boot_time - TIMEZONE_ADJUSTMENT);
    return MAX_BYTES_64_BIT_INT;
    
}

vfs_handles_t meminfo_handles = {
    .open = 0,
    .close = virt_file_generic_close,
    .read = virt_file_meminfo_read,
    .write = 0,
    .ioctl = 0,
    .seek = 0,
};

vfs_handles_t cpuinfo_handles = {
    .open = 0,
    .close = virt_file_generic_close,
    .read = virt_file_cpuinfo_read,
    .write = 0,
    .ioctl = 0,
    .seek = 0,
};

vfs_handles_t uptime_handles = {
    .open = 0,
    .close = virt_file_generic_close,
    .read = virt_file_uptime_read,
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
    create_virt_file(sysinfo,"cpuinfo",&cpuinfo_handles,PRIV_STD);
    create_virt_file(sysinfo,"uptime",&uptime_handles,PRIV_STD);
}