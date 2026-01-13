
#ifndef INCLUDE_UTIL_H
#define INCLUDE_UTIL_H

#define CEIL_DIV(a,b) (((a + b) - 1 )/ b)
#define ALIGN_DOWN(a, b) ((b) ? ((a) - ((a) % (b))) : (a))
#define ALIGN_UP(a,b) (a + b - 1) & ~(a - 1)
#include <stdint.h>
#define COMBINE_WORDS(lsb,msb) ((uint32_t)(msb) >> 16 | (lsb))
#define nullptr 0

typedef struct {
    uint32_t length;
    unsigned char* str;
} string_t;

typedef struct {
    uint64_t first;
    uint64_t second;
} uint64_pair_t;

typedef struct {
    uint32_t n_strings;
    string_t* strings;
}string_array_t;

/**
 * memset:
 * Sets n bytes of dest to val
 * @param dest The memory destination
 * @param val The value to be set to
 * @param n The number of bytes 
 * 
 * @return The destination
 */
void* memset(void* dest, int val, uint32_t n);

/**
 * memcpy:
 * Copies n bytes from src to dest
 * @param dest The memory destination
 * @param src The memory source
 * @param n The number of bytes to copy
 * 
 * @return The destination
 */
void* memcpy(void* dest,void* src, uint32_t n);

/**
 * memmove: 
 * Copies bytes from dest to src and ensures that the compiler does not do some weird shenanigans
 * @param dest The memory destination
 * @param src The memory source
 * @param n The number of bytes to copy
 * 
 * @return The destination
 */
void* memmove(void* dest, void* src, uint32_t n);
/**
 * streq:
 * Compares two null-terminated strings
 * @param str1 A pointer to the first string
 * @param str2 A pointer to the second string
 * 
 * @return 1 if both strings are equal
 *         0 if they are not equal 
 */
uint8_t streq(const unsigned char* str1, const unsigned char* str2);

/**
 * strneq: 
 * Compares two strings
 * @param str1 A pointer to the first string
 * @param str2 A pointer to the second string
 * @param len_1 The length of the first string
 * @param len_2 The length of the second string
 * 
 * @return 1 if both strings are equal
 *         0 if they are not equal 
 */
uint8_t strneq(const unsigned char* str1, const unsigned char* str2, uint32_t len_1, uint32_t len_2); 

/**
 * strlen: 
 * Returns the length of a null-terminated string
 * @param str The string
 * @return The length of the string
 */
uint32_t strlen(unsigned char* str);

/**
 * Frees a string array assuming the array, the strings and the char* are heap allocated
 * @param str_arr The string array
 */
void free_string_arr(string_array_t* str_arr);

/**
 * find_char: 
 * Finds the first occurance of a byte in a C-Style string and returns the index of it
 * @param str The string
 * @param c The character to find
 * @return The index of the character;
 * 
 *         (uint32_t)-1 of not found
 */
uint32_t find_char(unsigned char* str,unsigned char c);

/**
 * rfind_char: 
 * Finds the last occurance of a byte in a C-Style string and returns the index of it
 * @param str The string
 * @param c The character to find
 * @return The index of the character;
 * 
 *         (uint32_t)-1 of not found
 */
uint32_t rfind_char(unsigned char* str, unsigned char c);

#endif