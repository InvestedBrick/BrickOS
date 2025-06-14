#include "kmalloc.h"
#include "memory.h"
#include "../util.h"
#include "../io/log.h"
static unsigned int heap_size;
static unsigned int heap_allocated;
static unsigned char kmalloc_initialized = 0;

//NOTE: This allocator does currently not care about returning allocated pages to the page manager since I don't wanna deal with it
memory_block_t* head = 0;

void alloc_and_map_new_page(){
    if(heap_size + MEMORY_PAGE_SIZE > KERNEL_MALLOC_END) {panic("Kernel heap has run out of memory"); return;}
    unsigned int phys_addr = pmm_alloc_page_frame();
    unsigned int n_allocated_pages = CEIL_DIV(heap_size,MEMORY_PAGE_SIZE);
    unsigned int new_page_addr = KERNEL_MALLOC_START + n_allocated_pages * MEMORY_PAGE_SIZE;
    mem_map_page(new_page_addr,phys_addr,PAGE_FLAG_WRITE);
    heap_size += MEMORY_PAGE_SIZE;
}

memory_block_t* find_free_block(unsigned int size){
    memory_block_t* current = head;
    while (current != 0) {
        if (current->free && current->size >= size){
            return current;
        }
        current = current->next;
    }
    return (memory_block_t*)0;
}

void split_block(memory_block_t* block, unsigned int size){
    if (block->size > size + MEMORY_BLOCK_SIZE){
        memory_block_t* new_block = block + MEMORY_BLOCK_SIZE + size;
        new_block->size = block->size - size - MEMORY_BLOCK_SIZE;
        new_block->free = 1;
        new_block->next = block->next;

        block->size = size;
        block->next = new_block;
    }
}

void* kmalloc(unsigned int size){
    if (size <= 0) return 0;
    if (!kmalloc_initialized) { error("kmalloc has not been initialized"); return 0;}
    memory_block_t* block = find_free_block(size);
    if(block != 0){
        split_block(block,size);
        block->free = 0;
        heap_allocated += size + MEMORY_BLOCK_SIZE;
        return (void*)((char*)block + MEMORY_BLOCK_SIZE);
    }
    
    unsigned int total_size = size + MEMORY_BLOCK_SIZE;
    if (heap_allocated + total_size >= heap_size){
        unsigned int n_pages_to_alloc = CEIL_DIV(total_size,MEMORY_PAGE_SIZE);
        //allocate more pages if needed
        for(unsigned int i = 0; i < n_pages_to_alloc;i++){
            alloc_and_map_new_page();
        }
    }

    memory_block_t* last = head;
    while (last && last->next) last = last->next;
    block = (memory_block_t*) ((char*)KERNEL_MALLOC_START + heap_allocated);
    block->size = size;
    block->free = 0;
    block->next = 0;
    if (last) last->next = block; else head = block;
    heap_allocated += total_size;

    return (void*)((char*)block + MEMORY_BLOCK_SIZE);

}

void merge_blocks(memory_block_t* block){
    while (block->next && block->next->free ){
        block->size = block->size + MEMORY_BLOCK_SIZE + block->next->size;
        block->next = block->next->next;
        heap_allocated -= block->size + MEMORY_BLOCK_SIZE;
    }
}
void kfree(void* addr){
    if (addr == 0 || !kmalloc_initialized) return;

    // I hope you dont start freeing random pointers
    memory_block_t* block = (memory_block_t*)((char*)addr - MEMORY_BLOCK_SIZE);
    block->free = 1;

    merge_blocks(block);
}   

void init_kmalloc(unsigned int initial_heapsize){
    heap_size = 0;
    heap_allocated = 0;
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
    heap_size = new_page_top * MEMORY_PAGE_SIZE;
}