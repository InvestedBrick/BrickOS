#pragma once

#ifndef INCLUDE_KMALLOC_H
#define INCLUDE_KMALLOC_H

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
#endif