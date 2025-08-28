#include "stdutils.h"
#include "malloc.h"
uint32_t strlen(const char* str){
    uint32_t len = 0;
    while(str[len] != '\0'){len++;}
    return len;
}

uint8_t streq(const char* str1, const char* str2){
    uint32_t i = 0;
    while(1){
        if (str1[i] != str2[i]) return 0;

        if (str1[i] == '\0' && str2[i] == '\0') return 1;

        i++;
    }

    return 1;

}

void* memset(void* dest, int val, uint32_t n){
    unsigned char* p = dest;
    for(uint32_t i = 0; i < n;i++){
        p[i] = (uint8_t)val;
    }
    return dest;
}

void* memcpy(void* dest,void* src, uint32_t n){
    if (n == 0) return dest;
    unsigned char* _dest = dest;
    unsigned char* _src = src;

    for (uint32_t i = 0; i < n;i++){
        _dest[i] = _src[i];
    }

    return dest;
}

char* uint32_to_ascii(uint32_t value) {
    char* buffer = (char*)malloc(11);
    if (!buffer) return 0;

    if (value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return buffer;
    }

    int i = 10; 
    buffer[i] = '\0';
    i--;

    while (value > 0) {
        buffer[i] = '0' + (value % 10);
        value /= 10;
        i--;
    }

    int start = i + 1;
    int j = 0;
    while (buffer[start]) {
        buffer[j++] = buffer[start++];
    }
    buffer[j] = '\0';

    return buffer;
}