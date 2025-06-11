#pragma once

#ifndef INCLUDE_MULTIBOOT_H
#define INCLUDE_MULTIBOOT_H

typedef struct {
    unsigned int mod_start;
    unsigned int mod_end;
    unsigned int cmdline;
    unsigned int reserved;
} multiboot_module_t;

typedef struct {
    unsigned int tabsize;
    unsigned int strsize;
    unsigned int addr;
    unsigned int reserved;
} multiboot_aout_symbol_table_t;

typedef struct  {
    unsigned int num;
    unsigned int size;
    unsigned int addr;
    unsigned int shndx;
} multiboot_elf_section_header_table_t;

typedef struct {
    unsigned int flags;
    unsigned int mem_lower;
    unsigned int mem_upper;
    unsigned int boot_device;

    unsigned int cmdline;
    unsigned int mods_count;
    unsigned int mods_addr;

    union 
    {
       multiboot_aout_symbol_table_t aout_sym;
       multiboot_elf_section_header_table_t elf_sec;
    }u;

    unsigned int mmap_length;
    unsigned int mmap_addr;
    
    unsigned int drives_length;
    unsigned int drives_addr;

    unsigned int config_table;
    unsigned int boot_loader_name;

    unsigned int apm_table;

    unsigned int vbe_control_info;
    unsigned int vbe_mode_info;
    unsigned short vbe_mode;
    unsigned short vbe_interface_seg;
    unsigned short vbe_interface_off;
    unsigned short vbe_interface_len;

}multiboot_info_t;


typedef struct{
    unsigned int size;
    unsigned int addr_low;
    unsigned int addr_high;
    unsigned int len_low;
    unsigned int len_high;
#define MULTIBOOT_MEMORY_AVAILABLE           1   
#define MULTIBOOT_MEMORY_RESERVED            2   
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE    3           
#define MULTIBOOT_MEMORY_NVS                 4
#define MULTIBOOT_MEMORY_BADRAM              5
    unsigned int type;
}__attribute__((packed)) multiboot_mmap_entry_t;
#endif