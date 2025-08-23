#ifndef INCLUDE_MALLOC_H
#define INCLUDE_MALLOC_H
#include <stdint.h>
struct memory_block{
    uint32_t size;
    struct memory_block* next;
    uint8_t free;
}__attribute__((packed));

typedef struct memory_block memory_block_t; 

#define MEMORY_BLOCK_SIZE sizeof(memory_block_t)
#define MEMORY_PAGE_SIZE 0x1000

void* malloc(uint32_t size);

void free(void* addr);
#endif