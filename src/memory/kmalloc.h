
#ifndef INCLUDE_KMALLOC_H
#define INCLUDE_KMALLOC_H
#include <stdint.h>
struct memory_block{
    uint32_t size;
    struct memory_block* next;
    uint8_t free;
}__attribute__((packed));

typedef struct memory_block memory_block_t; 

#define MEMORY_BLOCK_SIZE sizeof(memory_block_t)

/**
 * init_kmalloc:
 * initializes the kernel memory allocator
 * 
 * @param initial_heapsize The initial heapsize in bytes
 */
void init_kmalloc(uint32_t initial_heapsize);

/**
 * set_heap_size:
 * Sets the size of the kernel heap
 * 
 * @param new_size The new heapsize
 */
void set_heap_size(uint32_t new_size);
/**
 * kmalloc:
 * Allocates some memory for the kernel
 * 
 * @param size The number of bytes to allocate
 * @return A pointer to the start of the allocated memory region
 */
void* kmalloc(uint32_t size);
/**
 * kfree:
 * Frees allocated kernel memory
 * @param addr The pointer to the memory location 
 */
void kfree(void* addr);
/**
 * realloc:
 * Reallocates a pointer and copies the data into a new size
 * @param ptr The pointer to the data
 * @param old_size The current size of the data 
 * @param new_size The new size of the pointer
 * 
 * @return The reallocated pointer
 */
void* realloc(void* ptr,uint32_t old_size,uint32_t new_size);
#endif