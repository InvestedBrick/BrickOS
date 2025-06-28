
#ifndef INCLUDE_UTIL_H
#define INCLUDE_UTIL_H

#define CEIL_DIV(a,b) (((a + b) - 1 )/ b)

typedef struct {
    unsigned int length;
    unsigned char* str;
} string_t;

typedef struct {
    unsigned int n_strings;
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
void* memset(void* dest, int val, unsigned int n);

/**
 * memcpy:
 * Copies n bytes from src to dest
 * @param dest The memory destination
 * @param src The memory source
 * @param n The number of bytes to copy
 * 
 * @return The destination
 */
void* memcpy(void* dest,void* src, unsigned int n);

/**
 * memmove: 
 * Copies bytes from dest to src and ensures that the compiler does not do some weird shenanigans
 * @param dest The memory destination
 * @param src The memory source
 * @param n The number of bytes to copy
 * 
 * @return The destination
 */
void* memmove(void* dest, void* src, unsigned int n);
/**
 * streq:
 * Compares two null-terminated strings
 * @param str1 A pointer to the first string
 * @param str2 A pointer to the second string
 * 
 * @return 1 if both strings are equal
 *         0 if they are not equal 
 */
unsigned char streq(const char* str1, const char* str2);

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
unsigned char strneq(const char* str1, const char* str2, unsigned int len_1, unsigned int len_2); 

/**
 * strlen: 
 * Returns the length of a null-terminated string
 * @param str The string
 * @return The length of the string
 */
unsigned int strlen(unsigned char* str);

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
 *         (unsigned int)-1 of not found
 */

unsigned int find_char(unsigned char* str,unsigned char c);
#endif