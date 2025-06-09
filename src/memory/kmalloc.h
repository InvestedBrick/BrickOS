#pragma once

#ifndef INCLUDE_KMALLOC_H
#define INCLUDE_KMALLOC_H

typedef struct{
    unsigned int size;
    memory_block_t* next;
    unsigned char free;
}__attribute__((packed)) memory_block_t;
#define MEMORY_BLOCK_SIZE sizeof(memory_block_t)

/**
 * init_kmalloc:
 * initializes the kernel memory allocator
 * 
 * @param initial_heapsize The initial heapsize in bytes
 */
void init_kmalloc(unsigned int initial_heapsize);

/**
 * set_heap_size:
 * Sets the size of the kernel heap
 * 
 * @param new_size The new heapsize
 */
void set_heap_size(unsigned int new_size);
/**
 * kmalloc:
 * Allocates some memory for the kernel
 * 
 * @param size The number of bytes to allocate
 * @return A pointer to the start of the allocated memory region
 */
void* kmalloc(unsigned int size);
/**
 * kfree:
 * Frees allocated kernel memory
 * @param addr The pointer to the memory location 
 */
void kfree(void* addr);
#endif