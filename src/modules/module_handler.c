#include "module_handler.h"
#include "../memory/kmalloc.h"
#include "../util.h"
#include "../filesystem/filesystem.h"
#include "../filesystem/file_operations.h"
#include "../io/log.h"
module_binary_t* module_binary_structs;
unsigned int module_count;

void save_module_binaries(multiboot_info_t* boot_info){
    
    module_count = boot_info->mods_count;
    module_binary_structs = kmalloc(module_count * sizeof(module_binary_t));

    if (!module_binary_structs) return;

    multiboot_module_t* modules = (multiboot_module_t*)boot_info->mods_addr;
    for (unsigned int i = 0; i < module_count;i++){
        module_binary_structs[i].size = modules[i].mod_end - modules[i].mod_start;
        
        unsigned char* cmd_line = (unsigned char*)modules[i].cmdline;
        // looks like "/modules/<module name>"
        unsigned int name_start_idx = find_char(&cmd_line[1],'/') + 2; // skip past the first '/' and return index to the first char of the name
        
        if (name_start_idx == (unsigned int)-1) 
            {error("Finding name for module failed");}
        else{
            unsigned int mod_name_len = strlen(&cmd_line[name_start_idx]);
            module_binary_structs[i].cmdline = (unsigned char*)kmalloc(mod_name_len + 1);
            memcpy(module_binary_structs[i].cmdline,&cmd_line[name_start_idx],mod_name_len);
            module_binary_structs[i].cmdline[mod_name_len] = '\0';
            
        }

        module_binary_structs[i].start = (unsigned int)kmalloc(module_binary_structs[i].size);

        // copy the module binary for future use
        memcpy((char*)module_binary_structs[i].start,(char*)modules[i].mod_start,module_binary_structs[i].size);

    }

}

void free_saved_module_binaries(){
    for (unsigned int i = 0; i < module_count;i++){
        kfree((void*)module_binary_structs[i].start);
    }
    kfree((void*)module_binary_structs);
}

void write_module_binaries_to_file(){
    for (unsigned int i = 0; i < module_count;i++){
        inode_t* module_dir = get_inode_by_full_file_path("root/modules/");
        
        inode_t* dir_save = change_active_dir(module_dir);
        if (!module_dir) 
            {error("Modules directory was not initialized"); return; }
        
        unsigned char* name = module_binary_structs[i].cmdline;
        unsigned int len = strlen(name);
        if (dir_contains_name(module_dir,name)){
            // if it is already here, remove it
            delete_file(module_dir,name);
        }

        int ret_code = create_file(module_dir,name,len,FS_TYPE_FILE);
        if (ret_code < 0) 
            {error("Failed to create module binary file"); continue;}

        int fd = open(name,FILE_FLAG_WRITE);
        if (fd == FILE_OP_FAILED) 
            {error("Failed to open module binary file"); continue;}

        ret_code = write(fd,(unsigned char*)module_binary_structs[i].start,module_binary_structs[i].size);

        if (ret_code < 0) 
            {error("Failed to write to module binary file"); continue;}
        
        ret_code = close(fd);
        if (ret_code == FILE_INVALID_FD) error("Failed to close module binary file");

        change_active_dir(dir_save);

        kfree((void*) name);
    }

    kfree((void*)module_binary_structs);
}
