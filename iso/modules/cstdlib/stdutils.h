
#ifndef INCLUDE_STDUTILS_H
#define INCLUDE_STDUTILS_H

#define CEIL_DIV(a,b) (((a + b) - 1 )/ b)

typedef struct {
    unsigned int length;
    unsigned char* str;
} string_t;

unsigned int strlen(const char* str);

void* memset(void* dest, int val, unsigned int n);

void* memcpy(void* dest,void* src, unsigned int n);

unsigned char streq(const char* str1, const char* str2);
#endif