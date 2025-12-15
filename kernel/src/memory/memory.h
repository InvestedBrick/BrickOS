
#ifndef INCLUDE_MEMORY_H
#define INCLUDE_MEMORY_H
#include <stdint.h>
#include "../filesystem/vfs/vfs.h"
#include "../kernel_header.h"

#define ENTRIES_PER_TABLE 512
#define TABLE_SIZE (ENTRIES_PER_TABLE * sizeof(uint64_t))
#define N_PML4_TABLES 256 
#define MEMORY_PAGE_SIZE 0x1000

#define HHDM                     (limine_data.hhdm) 
#define KERNEL_MALLOC_START      (HHDM + 0xd0000000) 
#define KERNEL_MALLOC_END        (HHDM + 0xf0000000) 
#define TEMP_KERNEL_COPY_ADDR    (KERNEL_MALLOC_END + MEMORY_PAGE_SIZE)

#define KERNEL_SHARED_MAPPING_IDX 256

#define PML4_SHIFT 39
#define PDPT_SHIFT 30
#define PD_SHIFT 21
#define PT_SHIFT 12

#define PML4E(a) (((a) >> PML4_SHIFT) & 0x1ff)
#define PDPTE(a) (((a) >> PDPT_SHIFT) & 0x1ff)
#define PDE(a) (((a) >> PD_SHIFT) & 0x1ff)
#define PTE(a) (((a) >> PT_SHIFT) & 0x1ff)

#define PTE_ADDR_MASK 0x000ffffffffff000
#define PTE_GET_ADDR(VALUE) ((VALUE) & PTE_ADDR_MASK)

#define SIGN_EXTEND_MASK 0xffff000000000000

#define CANONICALIZE(addr) ((((addr) & (1ull << 47)) ? (addr | SIGN_EXTEND_MASK) : (addr & ~SIGN_EXTEND_MASK)))


#define PAGE_FLAG_PRESENT 0x1
#define PAGE_FLAG_WRITE (1 << 1)
#define PAGE_FLAG_USER  (1 << 2)


#define INVALID_PHYS_ADDR (uint64_t)-1

#include "../vector.h"

typedef struct {
    uint64_t phys_addr;
    uint32_t ref_count;
} shared_page_t;

typedef struct{
    shared_page_t** shared_pages;
    uint32_t ref_count;
    uint32_t n_pages;
    uint32_t unique_id; // either inode id or unique dev id

}shared_object_t;

typedef struct virt_mem_area{
    void* addr;
    uint32_t size;
    uint32_t prot;
    uint32_t flags;

    uint32_t fd;
    uint32_t offset;

    shared_object_t* shrd_obj;
    struct virt_mem_area* next;

} virt_mem_area_t;

void init_shm_obj_vector();

void append_shared_object(shared_object_t* shrd_obj);

/**
 * find_shared_object_by_id:
 * returns a shared object with the given unique id if exists
 * @param unique_id The unique id of the shared object (inode id / dev id)
 * @return A pointer to the shared object or null if it does not yet exist
 */
shared_object_t* find_shared_object_by_id(uint32_t unique_id);

extern vector_t shm_obj_vec;

/**
 * find_virt_mem_area:
 * finds a virtual memory area in a virt_mem_area_t linked list, assuming addr is page-aligned
 * 
 * @param start The start of the linked list
 * @param addr The address to search
 * 
 * @return The virt_mem_area_t to which the addr belongs
 */
virt_mem_area_t* find_virt_mem_area(virt_mem_area_t* start,uint64_t addr);

/**
 * init_memory:
 * Sets up the physical and virtual memory manager
 */
void init_memory();

/**
 * invalidate:
 * invalidates virtual memory page
 * @param vaddr The address of the memory page
 */
void invalidate(uint64_t vaddr);

/**
 * pmm_alloc_page_frame:
 *  Allocates a page frame for the kernel
 * @return the physical address of the page frame
 */
uint64_t pmm_alloc_page_frame();
/**
 * mem_map_page:
 * Maps a virtual memory page to a physical address
 * 
 * @param virt_addr The virtual address of the memory page
 * @param phys_addr The physical address of the memory page
 * @param flags The flags for the page
 */
void mem_map_page(uint64_t virt_addr, uint64_t phys_addr, uint32_t flags);

/**
 * mem_get_current_pml4_table:
 * Returns the current pml4 table
 * @return A pointer to the pml4 table
 */
uint64_t* mem_get_current_pml4_table();

/**
 * mem_set_current_pml4_table:
 * changes the pml4 table
 * @param pml4_table The new pml4 table
 */
void mem_set_current_pml4_table(uint64_t* pml4_table);

/**
 * mem_set_current_pml4_phys:
 * Sets the current pml4 table
 * @param pml4_table The pml4 table
 */
void mem_set_current_pml4_phys(uint64_t* pml4_table);

/**
 * mem_get_current_pml4_phys:
 * Gets the current pml4 table
 * @return The pml4 table
 */
uint64_t mem_get_current_pml4_phys();

/**
 * create_user_pml4_table:
 * Creates a pml4 table for a user process
 * 
 * @return The pml4 table
 */
uint64_t* create_user_pml4_table();
/**
 * free_user_pml4_table:
 * Marks a user pml4 table as unused, frees all page table pointers, page tables and page frames within
 * 
 * @param user_pml4 The user pml4 table
 */
void free_user_pml4_table(uint64_t* user_pml4);

/**
 * void restore_kernel_pml4_table:
 * restores the current pml4 table to be the kernel pml4 table
 */
void restore_kernel_pml4_table();

/**
 * pmm_free_page_frame:
 * Frees a page frame
 * @param phys_addr The physical address of the page frame
 */
void pmm_free_page_frame(uint64_t phys_addr);

/**
 * mem_unmap_page:
 * Unmaps a memory page, but does not free the page frame (see pmm_free_page_frame)
 * @param virt_addr The virtual address to which the page was mapped
 */
void mem_unmap_page(uint64_t virt_addr);

/**
 * mem_map_page_in_pml4:
 * Map a memory page frame into a currently non-selected pml4 table
 * @param pml4_table The currently non-selected pml4 table
 * @param virt_addr The desired virtual address to map to
 * @param phys_addr The physical address of the page
 * @param flags The flags for the page
 */
void mem_map_page_in_pml4(uint64_t* pml4_table, uint64_t virt_addr, uint64_t phys_addr, uint32_t flags);


/**
 * flush_tlb:
 * Refreshes the translation lookaside buffer
 */
void flush_tlb();


void parse_and_log_limine_memory_mapping();
#endif