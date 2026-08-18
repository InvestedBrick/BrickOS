#ifndef INCLUDE_QUEUE_H
#define INCLUDE_QUEUE_H

#include <stdint.h>

typedef uint64_t queue_data_t;

typedef struct queue_entry {
    queue_data_t data;
    struct queue_entry* next;
}queue_entry_t;

typedef struct {
    uint32_t size;
    queue_entry_t* head; 
}queue_t;

/**
 * create_queue:
 * Allocates a queue_t struct and returns a pointer to it
 * @return The queue
 */
queue_t* create_queue();

/**
 * destroy_queue:
 * Cleans up a queue and all its enteies that was created by create_queue
 * @param queue The qeueue
 */
void destroy_queue(queue_t* queue);

/**
 * init_queue:
 * Sets up a queue
 * @param queue The queue
 */
void init_queue(queue_t* queue);

/**
 * queue_push:
 * pushes data to the back of a queue
 * @param queue The queue
 * @param data The data to enqueue
 */
void queue_push(queue_t* queue, queue_data_t data);

/**
 * queue_pop:
 * Removes and returns the first element in a queue
 * @param queue The queue
 */
queue_data_t queue_pop(queue_t* queue);

#endif