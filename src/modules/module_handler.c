#include "module_handler.h"
#include "../memory/kmalloc.h"
#include "../util.h"

module_binary_t* module_binary_structs;
unsigned int module_count;

void save_module_binaries(multiboot_info_t* boot_info){
    module_count = boot_info->mods_count;
    module_binary_structs = kmalloc(module_count * sizeof(module_binary_t));

    if (!module_binary_structs) return;

    multiboot_module_t* modules = (multiboot_module_t*)boot_info->mods_addr;
    for (unsigned int i = 0; i < module_count;i++){
        module_binary_structs[i].size = modules[0].mod_end - modules[0].mod_start;

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
