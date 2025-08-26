
#ifndef INCLUDE_STDUTILS_H
#define INCLUDE_STDUTILS_H

#define CEIL_DIV(a,b) (((a + b) - 1 )/ b)
#include <stdint.h>
typedef struct {
    uint32_t length;
    unsigned char* str;
} string_t;

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

uint32_t strlen(const char* str);

void* memset(void* dest, int val, uint32_t n);

void* memcpy(void* dest,void* src, uint32_t n);

uint8_t streq(const char* str1, const char* str2);
#endif