#include "queue.h"
#include "../memory/kmalloc.h"
#include <shared/util.h>

void init_queue(queue_t* queue){
    queue->head = nullptr;
    queue->size = 0;
}

queue_t* create_queue(){
    queue_t* queue = (queue_t*)kmalloc(sizeof(queue_t));
    memset(queue,0,sizeof(queue_t));
    return queue;
}

void destroy_queue(queue_t* queue) {

    while(queue->size > 0){
        queue_pop(queue);
    }

    kfree((void*)queue);
}

void queue_push(queue_t* queue, queue_data_t data){
    queue_entry_t* entry = (queue_entry_t*)kmalloc(sizeof(queue_entry_t));
    entry->data = data;
    entry->next = nullptr;

    queue_entry_t* curr = queue->head;
    if (!curr) queue->head = entry; 
    else {
        while(curr->next) curr = curr->next;
        curr->next = entry;
    }

    queue->size++;

}

queue_data_t queue_pop(queue_t* queue){
    if (!queue->size) return 0;

    queue_entry_t* entry = queue->head;
    queue->head = entry->next;

    queue_data_t data = entry->data;
    kfree(entry);
    queue->size--;
    return data;
}