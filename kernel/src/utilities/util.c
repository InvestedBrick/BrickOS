#include "util.h"
#include "vector.h"
#include "../memory/kmalloc.h"
#include "../io/log.h"
#include <stddef.h>

void* memmove(void* dest, void* src, uint32_t n) {
    if (n == 0) return dest;

    unsigned char* _dest = dest;
    unsigned char* _src = src;

    if (_dest < _src) {
        // Forward copy
        for (uint32_t i = 0; i < n; i++) {
            _dest[i] = _src[i];
        }
    } else if (_dest > _src) {
        // Backward copy
        for (uint32_t i = n; i > 0; i--) {
            _dest[i - 1] = _src[i - 1];
        }
    }

    return dest;
}

uint8_t streq(const unsigned char* str1, const unsigned char* str2){
    uint32_t i = 0;
    while(1){
        if (str1[i] != str2[i]) return 0;

        if (str1[i] == '\0' && str2[i] == '\0') return 1;

        i++;
    }

    //how did we get here?
    return 1;

}
uint8_t strneq(const unsigned char* str1, const unsigned char* str2, uint32_t len_1, uint32_t len_2){
    uint32_t max_len = len_1 > len_2 ? len_1 : len_2;

    for (uint32_t i = 0; i < max_len;i++){
        if (str1[i] != str2[i]) return 0;

        if (str1[i] == '\0' && str2[i] == '\0') return 1;
    }
    return 1;
}

void free_string_arr(string_array_t* str_arr){
    for (uint32_t i = 0; i < str_arr->n_strings;i++){
        kfree(str_arr->strings[i].str);
    }
    if (str_arr->n_strings > 0) kfree(str_arr->strings);
    kfree(str_arr);
}

uint32_t rfind_char(unsigned char* str, unsigned char c){
    uint32_t str_len = strlen(str);

    for (int i = str_len - 1; i >= 0; i--){
        if (str[i] == c) return (uint32_t)i;
    }

    return (uint32_t)-1;
}

int memcmp(const void* ptr1, const void* ptr2, size_t num) {
    const unsigned char* p1 = ptr1;
    const unsigned char* p2 = ptr2;

    for (size_t i = 0; i < num; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }
    return 0;
}

shared_addr_t* shared_address_find(vector_t* vec, void* addr){
    for (uint32_t i = 0; i < vec->size; i++){
        shared_addr_t* shrd_addr = (shared_addr_t*)vec->data[i];
        if (shrd_addr->addr == addr){
            return shrd_addr;
        }
    }
    return nullptr;
}

bool shared_address_add(vector_t* vec,void* addr){

    shared_addr_t* shrd_addr = shared_address_find(vec,addr);
    if (!shrd_addr){
        shrd_addr = (shared_addr_t*)kmalloc(sizeof(shared_addr_t));
        shrd_addr->addr = addr;
        shrd_addr->cntr = 1;
        vector_append(vec,(vector_data_t)shrd_addr);
        return true;
    }
    shrd_addr->cntr++;
    return false;
}

bool shared_address_remove(vector_t* vec, void* addr){
    shared_addr_t* shrd_addr = shared_address_find(vec,addr);
    if (!shrd_addr) return false;

    shrd_addr->cntr--;
    if (shrd_addr->cntr == 0){
        // last address holder
        vector_erase_item(vec,(vector_data_t)shrd_addr);
        kfree(shrd_addr);
        return true;
    }
    return false;
}