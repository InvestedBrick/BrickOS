#include "kmalloc.h"
#include "memory.h"
#include "../util.h"
#include "../io/log.h"
static unsigned int heap_start;
static unsigned int heap_size;
static unsigned int threshold;
static unsigned char kmalloc_initialized = 0;

void init_kmalloc(unsigned int initial_heapsize){
    heap_start = KERNEL_MALLOC_START;
    heap_size = 0;
    threshold = 0;
    kmalloc_initialized = 1;

    set_heap_size(initial_heapsize);
}

void set_heap_size(unsigned int new_size){
    unsigned int old_page_top = CEIL_DIV(heap_size,MEMORY_PAGE_SIZE);

    unsigned int new_page_top = CEIL_DIV(new_size,MEMORY_PAGE_SIZE);

    unsigned int diff = new_page_top - old_page_top;

    for (unsigned int i = 0;i < diff;i++){
        unsigned int phys = pmm_alloc_page_frame();
        mem_map_page(KERNEL_MALLOC_START + old_page_top * MEMORY_PAGE_SIZE + i * MEMORY_PAGE_SIZE,
                    phys, PAGE_FLAG_WRITE);
    }
}