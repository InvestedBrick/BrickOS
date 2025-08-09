#ifndef INCLUDE_MALLOC_H
#define INCLUDE_MALLOC_H

struct memory_block{
    unsigned int size;
    struct memory_block* next;
    unsigned char free;
}__attribute__((packed));

typedef struct memory_block memory_block_t; 

#define MEMORY_BLOCK_SIZE sizeof(memory_block_t)
#define MEMORY_PAGE_SIZE 0x1000

void* malloc(unsigned int size);

void free(void* addr);
#endif