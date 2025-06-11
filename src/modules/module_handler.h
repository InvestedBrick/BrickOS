#pragma once

#ifndef MODULE_HANDLER_H
#define MODULE_HANDLER_H

#include "../multiboot.h"
typedef struct {
    unsigned int size;
    unsigned int start;
}module_binary_t;


extern module_binary_t* module_binary_structs;
extern unsigned int module_count;

/**
 * save_module_binaries:
 * Saves the module binaries provided by the boot info and their starts to module_binary_t structs and then lists them in an array called "module_binary_structs"
 * @param boot_info The boot info
 */
void save_module_binaries(multiboot_info_t* boot_info);

/**
 * free_saved_module_binaries: 
 * Frees the allocated memory used for the "module_binary_structs" array
 */
void free_saved_module_binaries();
#endif