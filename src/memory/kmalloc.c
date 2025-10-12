#include "kmalloc.h"
#include "memory.h"
#include "../util.h"
#include "../io/log.h"
#include <stdint.h>
static uint64_t heap_size;
static uint8_t kmalloc_initialized = 0;

//NOTE: This allocator does currently not care about returning allocated pages to the page manager since I don't wanna deal with it
memory_block_t* head = 0;

void alloc_and_map_new_page(){
    if(heap_size + MEMORY_PAGE_SIZE > KERNEL_MALLOC_END) {panic("Kernel heap has run out of memory"); return;}
    uint64_t phys_addr = pmm_alloc_page_frame();
    uint32_t n_allocated_pages = CEIL_DIV(heap_size,MEMORY_PAGE_SIZE);
    uint64_t new_page_addr = KERNEL_MALLOC_START + n_allocated_pages * MEMORY_PAGE_SIZE;
    mem_map_page(new_page_addr,phys_addr,PAGE_FLAG_WRITE);
    heap_size += MEMORY_PAGE_SIZE;
}

void merge_blocks(memory_block_t* block){
    while (block->next && block->next->free ){
        block->size = block->size + MEMORY_BLOCK_SIZE + block->next->size;
        block->next = block->next->next;
    }
}

void defrag_heap(){
    memory_block_t* current = head;
    while (current){
        if (current->free){
            merge_blocks(current);
        }
        current = current->next;
    }
}

memory_block_t* find_free_block(uint32_t size){
    memory_block_t* current = head;
    while (current) {
        if (current->free && current->size >= size){
            return current;
        }
        current = current->next;
    }
    return (memory_block_t*)0;
}

void split_block(memory_block_t* block, uint32_t size){
    if (block->size > size + MEMORY_BLOCK_SIZE){
        memory_block_t* new_block = (memory_block_t*)((char*)block + MEMORY_BLOCK_SIZE + size);
        new_block->size = block->size - size - MEMORY_BLOCK_SIZE;
        new_block->free = 1;
        new_block->next = block->next;

        block->size = size;
        block->next = new_block;
    }
}

void* kmalloc(uint32_t size){
    if (size <= 0) return 0;
    if (!kmalloc_initialized) { error("kmalloc has not been initialized"); return 0;}
    memory_block_t* block = find_free_block(size);
    if(block != 0){
        split_block(block,size);
        block->free = 0;
        return (void*)((char*)block + MEMORY_BLOCK_SIZE);
    }
    
    // no large enough block exists -> allocate more space
    uint32_t total_size = size + MEMORY_BLOCK_SIZE;
    uint32_t n_pages_to_alloc = CEIL_DIV(total_size,MEMORY_PAGE_SIZE);
    //allocate more pages if needed
    for(uint32_t i = 0; i < n_pages_to_alloc;i++){
        alloc_and_map_new_page();
    }

    memory_block_t* last = head;
    while (last && last->next) last = last->next;
    block = (memory_block_t*) ((char*)last + MEMORY_BLOCK_SIZE + last->size);
    block->size = size;
    block->free = 0;
    block->next = 0;
    if (last) last->next = block; else head = block;

    return (void*)((char*)block + MEMORY_BLOCK_SIZE);

}


void kfree(void* addr){
    if (addr == 0 || !kmalloc_initialized) return;

    // I hope you dont start freeing random pointers
    memory_block_t* block = (memory_block_t*)((char*)addr - MEMORY_BLOCK_SIZE);
    
    if (block->free) panic("Double free detected");
    
    block->free = 1;

    defrag_heap();
}   

void init_kmalloc(uint32_t initial_heapsize){
    heap_size = 0;
    kmalloc_initialized = 1;
    set_heap_size(initial_heapsize);

    // Initialize the first block
    head = (memory_block_t*) KERNEL_MALLOC_START;
    head->size = heap_size - MEMORY_BLOCK_SIZE;
    head->free = 1;
    head->next = 0;
}

void set_heap_size(uint32_t new_size){
    uint32_t old_page_top = CEIL_DIV(heap_size,MEMORY_PAGE_SIZE);

    uint32_t new_page_top = CEIL_DIV(new_size,MEMORY_PAGE_SIZE);
    uint32_t diff = new_page_top - old_page_top;
    for (uint32_t i = 0;i < diff;i++){
        uint32_t phys = pmm_alloc_page_frame();
    
        mem_map_page(KERNEL_MALLOC_START + old_page_top * MEMORY_PAGE_SIZE + i * MEMORY_PAGE_SIZE,
                    phys, PAGE_FLAG_WRITE);
    }
    heap_size = new_page_top * MEMORY_PAGE_SIZE;
}

void* realloc(void* ptr,uint32_t old_size,uint32_t new_size){
    if (!ptr) return kmalloc(new_size);

    if (new_size == 0) {
        kfree(ptr);
        return 0;
    }

    void* new_ptr = kmalloc(new_size);
    uint32_t copy_size = old_size < new_size ? old_size : new_size;
    memcpy(new_ptr, ptr, copy_size);
    kfree(ptr);

    return new_ptr;
}