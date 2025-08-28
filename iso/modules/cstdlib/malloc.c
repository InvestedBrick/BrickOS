#include "malloc.h"
#include "syscalls.h"
#include "stdutils.h"
memory_block_t* malloc_head = 0;

memory_block_t* find_free_block(uint32_t size){
    memory_block_t* current = malloc_head;
    while (current != 0) {
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

void merge_blocks(memory_block_t* block){
    while (block->next && block->next->free ){
        block->size = block->size + MEMORY_BLOCK_SIZE + block->next->size;
        block->next = block->next->next;
    }
}

void* malloc(uint32_t size){
    if (size <= 0) return 0;
    
    memory_block_t* block = find_free_block(size);
    
    if (!block){
        memory_block_t* last = malloc_head;
        while (last && last->next) last = last->next;

        uint32_t total_size = size + MEMORY_BLOCK_SIZE;
        block = (memory_block_t*)mmap(total_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS,MAP_FD_NONE,0);
        block->free = 1;
        block->next = 0;
        block->size = CEIL_DIV(total_size,MEMORY_PAGE_SIZE) * MEMORY_BLOCK_SIZE;

        if(last) last->next = block; else malloc_head = block;
    }

    split_block(block,size);
    block->free = 0;
    return (void*)((char*)block + MEMORY_BLOCK_SIZE);
}

void free(void* addr){
    if(!addr) return;

    // I hope you dont start freeing random pointers
    memory_block_t* block = (memory_block_t*)((char*)addr - MEMORY_BLOCK_SIZE);

    if(block->free) return;

    block->free = 1;
    merge_blocks(block);

}