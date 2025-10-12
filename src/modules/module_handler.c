#include "module_handler.h"
#include "../memory/kmalloc.h"
#include "../util.h"
#include "../filesystem/filesystem.h"
#include "../filesystem/file_operations.h"
#include "../tables/syscalls.h"
#include "../io/log.h"
#include "../kernel_header.h"
module_binary_t* module_binary_structs;
uint32_t module_count;

void save_module_binaries(multiboot_info_t* boot_info){
    
    module_count = boot_info->mods_count;
    module_binary_structs = kmalloc(module_count * sizeof(module_binary_t));

    if (!module_binary_structs) return;

    multiboot_module_t* modules = (multiboot_module_t*)(uint64_t)boot_info->mods_addr;
    for (uint32_t i = 0; i < module_count;i++){
        module_binary_structs[i].size = modules[i].mod_end - modules[i].mod_start;
        
        unsigned char* cmd_line = (unsigned char*)(uint64_t)modules[i].cmdline;
        // looks like "/modules/<module name>"
        uint32_t name_start_idx = find_char(&cmd_line[1],'/') + 2; // skip past the first '/' and return index to the first char of the name
        
        if (name_start_idx == ((uint32_t)-1) + 2) 
            {error("Finding name for module failed");}
        else{
            uint32_t mod_name_len = strlen(&cmd_line[name_start_idx]);
            module_binary_structs[i].cmdline = (unsigned char*)kmalloc(mod_name_len + 1);
            memcpy(module_binary_structs[i].cmdline,&cmd_line[name_start_idx],mod_name_len);
            module_binary_structs[i].cmdline[mod_name_len] = '\0';
            
        }

        module_binary_structs[i].start = (uint64_t)kmalloc(module_binary_structs[i].size);

        // copy the module binary for future use
        memcpy((unsigned char*)module_binary_structs[i].start,(unsigned char*)(uint64_t)modules[i].mod_start,module_binary_structs[i].size);

    }

}

void write_module_binaries_to_file(){
    for (uint32_t i = 0; i < module_count;i++){
        inode_t* module_dir = get_inode_by_full_file_path("modules/");
        
        if (!module_dir) 
            {error("Modules directory was not initialized"); return; }
        
        inode_t* dir_save = change_active_dir(module_dir);
        
        unsigned char* name = (unsigned char*)module_binary_structs[i].cmdline;
        uint32_t len = strlen(name);
        if (dir_contains_name(module_dir,name)){
            // if it is already here, remove it
            sys_rmfile(name);
        }

        int ret_code = create_file(module_dir,name,len,FS_TYPE_FILE,FS_FILE_PERM_WRITABLE | FS_FILE_PERM_READABLE,PRIV_STD);
        if (ret_code < 0) 
            {error("Failed to create module binary file"); continue;}

        int fd = sys_open(&global_kernel_process,name,FILE_FLAG_WRITE);
        if (fd < 0) 
            {error("Failed to open module binary file"); continue;}

        ret_code = sys_write(&global_kernel_process,fd,(unsigned char*)module_binary_structs[i].start,module_binary_structs[i].size);
        if (ret_code < 0) 
            {error("Failed to write to module binary file"); continue;}
        
        ret_code = sys_close(&global_kernel_process,fd);
        if (ret_code == FILE_INVALID_FD) error("Failed to close module binary file");

        change_file_permissions(name, FS_FILE_PERM_EXECUTABLE | FS_FILE_PERM_READABLE);

        change_active_dir(dir_save);

        kfree((void*) name);
    }

    kfree((void*)module_binary_structs);
}
