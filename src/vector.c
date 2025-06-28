#include "vector.h"
#include "util.h"
#include "memory/kmalloc.h"
void vector_resize(vector_t* vec,unsigned int new_capacity){
    unsigned int* new_data = kmalloc(sizeof(unsigned int) * new_capacity);
    memcpy(new_data,vec->data,vec->size * sizeof(unsigned int));
    kfree(vec->data);

    vec->data = new_data;
    vec->capacity = new_capacity;
}

void vector_append(vector_t* vec, unsigned int data){
    if (vec->size >= vec->capacity){
        vector_resize(vec,vec->capacity * 2);
    }

    vec->data[vec->size] = data;
    vec->size++;
}
unsigned int vector_pop(vector_t* vec){
    if (!vector_is_empty(vec)){
        if (vec->size - 1 <= vec->capacity * 0.25f){
            vector_resize(vec,vec->capacity * 0.5);
        }
        vec->size--;
        return vec->data[vec->size];
    }
    return 0;
}

unsigned char vector_is_empty(vector_t* vec) {
    return vec->size == 0;
}

unsigned int vector_erase(vector_t* vec,unsigned int idx){
    unsigned int erased_item = vec->data[idx];

    for(unsigned int i = idx; i < vec->size - 1;i++){
        vec->data[i] = vec->data[i + 1];
    }

    vec->size--;
    return erased_item;
}

unsigned int vector_find(vector_t* vec, unsigned int data){
    for(unsigned int i = 0; i < vec->size;i++){
        if (vec->data[i] == data) return i;
    }

    return (unsigned int)-1;
}

void init_vector(vector_t* vec){
    vec->size = 0;
    vec->capacity = 1;
    vec->data = kmalloc(sizeof(unsigned int));
}


void vector_free(vector_t* vec, unsigned char ptrs){
    if (ptrs){
        for (unsigned int i = 0; i < vec->size;i++){
            kfree((void*)vec->data[i]);
        }
    }
    kfree((void*)vec->data);
}
