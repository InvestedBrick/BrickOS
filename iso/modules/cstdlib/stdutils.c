#include "stdutils.h"

unsigned int strlen(const char* str){
    unsigned int len = 0;
    while(str[len] != '\0'){len++;}
    return len;
}

unsigned char streq(const char* str1, const char* str2){
    unsigned int i = 0;
    while(1){
        if (str1[i] != str2[i]) return 0;

        if (str1[i] == '\0' && str2[i] == '\0') return 1;

        i++;
    }

    return 1;

}

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