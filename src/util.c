#include "util.h"
#include "memory/kmalloc.h"
#include "io/log.h"
// TODO: move the vector stuff out of here
void* memset(void* dest, int val, unsigned int n){
    unsigned char* p = dest;
    for(unsigned int i = 0; i < n;i++){
        p[i] = (unsigned char)val;
    }
    return dest;
}

void* memcpy(void* dest,void* src, unsigned int n){
    if (n == 0) return dest;
    unsigned char* _dest = dest;
    unsigned char* _src = src;

    for (unsigned int i = 0; i < n;i++){
        _dest[i] = _src[i];
    }

    return dest;
}

void* memmove(void* dest, void* src, unsigned int n) {
    if (n == 0) return dest;

    unsigned char* _dest = dest;
    unsigned char* _src = src;

    if (_dest < _src) {
        // Forward copy
        for (unsigned int i = 0; i < n; i++) {
            _dest[i] = _src[i];
        }
    } else if (_dest > _src) {
        // Backward copy
        for (unsigned int i = n; i > 0; i--) {
            _dest[i - 1] = _src[i - 1];
        }
    }

    return dest;
}

unsigned char streq(const char* str1, const char* str2){
    unsigned int i = 0;
    while(1){
        if (str1[i] != str2[i]) return 0;

        if (str1[i] == '\0' && str2[i] == '\0') return 1;

        i++;
    }

    //how did we get here?
    return 1;

}
unsigned char strneq(const char* str1, const char* str2, unsigned int len_1, unsigned int len_2){
    unsigned int max_len = len_1 > len_2 ? len_1 : len_2;

    for (unsigned int i = 0; i < max_len;i++){
        if (str1[i] != str2[i]) return 0;

        if (str1[i] == '\0' && str2[i] == '\0') return 1;
    }
    return 1;
}
unsigned int strlen(unsigned char* str){
    unsigned int len = 0;
    while(str[len] != 0){len++;}
    return len;
}

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

void free_string_arr(string_array_t* str_arr){
    for (unsigned int i = 0; i < str_arr->n_strings;i++){
        kfree(str_arr->strings[i].str);
    }
    kfree(str_arr->strings);
    kfree(str_arr);
}

unsigned int find_char(unsigned char* str,unsigned char c){
    for(unsigned int i = 0; str[i] != '\0';i++){
        if (str[i] == c) return i;
    }
    return (unsigned int)-1;
}