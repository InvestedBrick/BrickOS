
#ifndef INCLUDE_STDUTILS_H
#define INCLUDE_STDUTILS_H

#define CEIL_DIV(a,b) (((a + b) - 1 )/ b)

unsigned int strlen(const char* str);

void* memset(void* dest, int val, unsigned int n);

void loop();

unsigned char streq(const char* str1, const char* str2);
#endif