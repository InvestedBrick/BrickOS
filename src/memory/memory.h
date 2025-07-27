
#ifndef INCLUDE_MEMORY_H
#define INCLUDE_MEMORY_H

#define KERNEL_START 0xc0000000 
#define KERNEL_MALLOC_START 0xd0000000 // give the kernel some space
#define KERNEL_MALLOC_END 0xf0000000 
#define TEMP_KERNEL_COPY_ADDR 0xf0000000
#define REC_PAGE_DIR ((unsigned int*)0xfffff000)
#define REC_PAGE_TABLE(i) ((unsigned int*) (0xffc00000) + ((i) * 0x400))

#define MEMORY_PAGE_SIZE 0x1000

#define PAGE_FLAG_PRESENT 0x1
#define PAGE_FLAG_WRITE (1 << 1)
#define PAGE_FLAG_OWNER (1 << 9)
#define PAGE_FLAG_USER  (1 << 2)

#define NUM_PAGE_DIRS 256
#define NUM_PAGE_FRAMES (0x20000000 / 0x1000 / 0x8)

// has to match with the number of identity mapped page tables in entry.asm
#define RESERVED_PAGE_TABLES 1

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
 * @param flags The flags for the page
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

/**
 * create_user_page_dir:
 * Creates a page directory for a user process
 * 
 * @return The page directory
 */
unsigned int* create_user_page_dir();
/**
 * free_user_page_dir:
 * Marks a user page directory as unused, frees all page tables and page frames within
 * 
 * @param usr_pd The user page directory
 */
void free_user_page_dir(unsigned int* usr_pd);

/**
 * void restore_kernel_memory_page_dir:
 * restores the current page directory to be the kernel page directory
 */
void restore_kernel_memory_page_dir();
/**
 * pmm_free_page_frame:
 * Frees a page frame
 * @param phys_addr The physical address of the page frame
 */
void pmm_free_page_frame(unsigned int phys_addr);

/**
 * mem_unmap_page:
 * Unmaps a memory page, but does not free the page frame (see pmm_free_page_frame)
 * @param virt_addr The virtual address to which the page was mapped
 */
void mem_unmap_page(unsigned int virt_addr);

/**
 * mem_map_page_in_dir:
 * Map a memory page frame into a currently non-selected page directory
 * @param page_dir The currently non-selected page directory
 * @param virt_addr The desired virtual address to map to
 * @param phys_addr The physical address of the page
 * @param flags The flags for the page
 */
void mem_map_page_in_dir(unsigned int* page_dir, unsigned int virt_addr, unsigned int phys_addr, unsigned int flags);

/**
 * un_identity_map_first_page_table:
 * Do you really need an explanation for this?
 */
void un_identity_map_first_page_table();
#endif