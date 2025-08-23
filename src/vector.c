#include "vector.h"
#include "util.h"
#include "memory/kmalloc.h"
#include <stdint.h>
void vector_resize(vector_t* vec,uint32_t new_capacity){
    uint32_t* new_data = kmalloc(sizeof(uint32_t) * new_capacity);
    memcpy(new_data,vec->data,vec->size * sizeof(uint32_t));
    kfree(vec->data);

    vec->data = new_data;
    vec->capacity = new_capacity;
}

void vector_append(vector_t* vec, uint32_t data){
    if (vec->size >= vec->capacity){
        vector_resize(vec,vec->capacity * 2);
    }

    vec->data[vec->size] = data;
    vec->size++;
}
uint32_t vector_pop(vector_t* vec){
    if (!vector_is_empty(vec)){
        if (vec->size - 1 <= vec->capacity * 0.25f){
            vector_resize(vec,vec->capacity * 0.5);
        }
        vec->size--;
        return vec->data[vec->size];
    }
    return 0;
}

uint8_t vector_is_empty(vector_t* vec) {
    return vec->size == 0;
}

uint32_t vector_erase(vector_t* vec,uint32_t idx){
    uint32_t erased_item = vec->data[idx];

    for(uint32_t i = idx; i < vec->size - 1;i++){
        vec->data[i] = vec->data[i + 1];
    }

    vec->size--;
    return erased_item;
}

uint32_t vector_find(vector_t* vec, uint32_t data){
    for(uint32_t i = 0; i < vec->size;i++){
        if (vec->data[i] == data) return i;
    }

    return (uint32_t)-1;
}

void init_vector(vector_t* vec){
    vec->size = 0;
    vec->capacity = 1;
    vec->data = kmalloc(sizeof(uint32_t));
}


void vector_free(vector_t* vec, uint8_t ptrs){
    if (ptrs){
        for (uint32_t i = 0; i < vec->size;i++){
            kfree((void*)vec->data[i]);
        }
    }
    kfree((void*)vec->data);
}
