#pragma once
#ifndef INCLUDE_MEMORY_H
#define INCLUDE_MEMORY_H

#define KERNEL_START 0xc0000000 
#define KERNEL_MALLOC_START 0xd0000000 // give the kernel some space
#define REC_PAGE_DIR ((unsigned int*)0xfffff000)
#define REC_PAGE_TABLE(i) ((unsigned int*) (0xffc00000) + (0x400 * i))

#define MEMORY_PAGE_SIZE 0x1000

#define PAGE_FLAG_PRESENT 0x1
#define PAGE_FLAG_WRITE (1 << 1)
#define PAGE_FLAG_OWNER (1 << 9)

#define NUM_PAGE_DIRS 256
#define NUM_PAGE_FRAMES (0x10000000 / 0x1000 / 0x8)

extern unsigned int initial_page_dir[1024];

/**
 * init_memory:
 * sets up memory pages
 * @param physical_alloc_start The calculated start of the phsyical allocations
 * @param mem_high The upper memory provided by the boot info
 */
void init_memory(unsigned int physical_alloc_start, unsigned int mem_high);

/**
 * invalidate:
 * invalidates virtual memory page
 * @param vaddr The address of the memory page
 */
void invalidate(unsigned int vaddr);

/**
 * pmm_alloc_page_frame:
 *  Allocates a page frame for the kernel
 * @return the physical address of the page frame
 */
unsigned int pmm_alloc_page_frame();
/**
 * mem_map_page:
 * Maps a virtual memory page to a physical address
 * 
 * @param virt_addr The virtual address of the memory page
 * @param phys_addr The physical address of the memory page
 * @param flags The flags (R/W/X) for the page
 */
void mem_map_page(unsigned int virt_addr, unsigned int phys_addr, unsigned int flags);

/**
 * mem_get_current_page_dir:
 * Returns the current page directory
 * @return A pointer to the page dir
 */
unsigned int* mem_get_current_page_dir();

/**
 * mem_change_page_dir:
 * changes the page directory
 * @param pd The new page directory
 */
void mem_change_page_dir(unsigned int* pd);
/**
 * mem_set_current_page_dir:
 * Sets the current page directory
 * @param pd The page directory
 */
void mem_set_current_page_dir(unsigned int* pd);

/**
 * sync_page_dirs:
 * synchronices the page directories
 */
void sync_page_dirs();
#endif