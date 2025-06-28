#include "util.h"
#include "memory/kmalloc.h"
#include "io/log.h"

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