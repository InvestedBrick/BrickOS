#pragma once

#ifndef INCLUDE_GDT_H
#define INCLUDE_GDT_H

typedef struct {
    unsigned short size;
    unsigned int address;
} __attribute__((packed)) gdt_t;

typedef struct {
    unsigned short limit_low;
    unsigned short base_low;
    unsigned char base_middle;
    unsigned char access;
    unsigned char granularity;
    unsigned char base_high;
}__attribute__((packed)) gdt_entry_t;

#define KERNEL_CODE_SEGMENT 0x9a
#define KERNEL_DATA_SEGMENT 0x92

#define USER_CODE_SEGMENT 0xfa
#define USER_DATA_SEGNENT 0xf2
#define GRANULARITY 0xcf

#define N_GDT_ENTRIES 5
/**
 * load_gdt - loads global descriptor table
 * @param table A pointer to a gdt
 * 
 */
void load_gdt(const gdt_t* table);
/**
 * set_gdt_entry:
 * creates a gdt entry with the given parameters
 * @param num The index of the gdt entry
 * @param base The start of the memory segment
 * @param limit The end of the memory segment
 * @param access The access of the memory segment
 * @param gran The granularity of the memroy segment
 */
void set_gdt_entry(int num,unsigned int base, unsigned int limit, unsigned char access, unsigned char gran);

/**
 * init_gdt:
 * Initializes a global descriptor table with 3 segments:
 *    - null segment
 *    - code segment
 *    - data segment
 */
void init_gdt();

#endif
