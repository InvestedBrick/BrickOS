
#ifndef MODULE_HANDLER_H
#define MODULE_HANDLER_H

#include "../multiboot.h"
#include "../../limine-protocol/include/limine.h"
typedef struct {
    uint32_t size;
    uint64_t start;
    unsigned char* cmdline;
}module_binary_t;


extern module_binary_t* module_binary_structs;
extern uint32_t module_count;

/**
 * save_module_binaries:
 * Saves the module binaries provided by limine and their starts to module_binary_t structs and then lists them in an array called "module_binary_structs"
 * @param boot_info The boot info
 */
void save_module_binaries(struct limine_module_response* module_response);

/**
 * write_module_binaries_to_file: 
 * Writes the module binaries to files in the modules/ folders
 * IMPORTANT: This function frees the binaries from memory, so the struct pointer becomes invalid
 */
void write_module_binaries_to_file();
#endif