
#ifndef INCLUDE_GDT_H
#define INCLUDE_GDT_H
#include <stdint.h>
typedef struct {
    uint16_t size;
    uint32_t address;
} __attribute__((packed)) gdt_t;

typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
}__attribute__((packed)) gdt_entry_t;

#define KERNEL_CODE_SEGMENT 0x9a
#define KERNEL_DATA_SEGMENT 0x92

#define USER_CODE_SEGMENT 0xfa
#define USER_DATA_SEGMENT 0xf2
#define GRANULARITY 0xcf

#define N_GDT_ENTRIES 6
/**
 * load_gdt - loads global descriptor table
 * @param table A pointer to a gdt
 * 
 */
void load_gdt(const gdt_t* table);
/**
 * 
 * load_tss - loads the task state segment
 * @param tss_segment The TSS segment index (should be 0x28)
 */
void load_tss(uint32_t tss_segment);
/**
 * set_gdt_entry:
 * creates a gdt entry with the given parameters
 * @param num The index of the gdt entry
 * @param base The start of the memory segment
 * @param limit The end of the memory segment
 * @param access The access of the memory segment
 * @param gran The granularity of the memory segment
 */
void set_gdt_entry(int num,uint32_t base, uint32_t limit, uint8_t access, uint8_t gran);

/**
 * init_gdt:
 * Initializes a global descriptor table with 3 segments:
 *    - null segment
 *    - code segment
 *    - data segment
 */
void init_gdt();

#endif
