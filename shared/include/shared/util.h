
#ifndef INCLUDE_UTIL_H
#define INCLUDE_UTIL_H

#define CEIL_DIV(a,b) (((a + b) - 1 )/ b)
#define ALIGN_DOWN(a, b) ((b) ? ((a) - ((a) % (b))) : (a))
#define ALIGN_UP(a, b) (((a) + (b) - 1) & ~((b) - 1))
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#define COMBINE_WORDS(lsb,msb) ((uint32_t)(msb) >> 16 | (lsb))
#define nullptr 0

#define STR2(x) #x
#define STR(x) STR2(x)

typedef struct {
    uint32_t length;
    unsigned char* str;
} string_t;

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

/**
 * memcmp:
 * Compares two memory regions 
 * @param ptr1 The first memory region
 * @param ptr2 The second memory region
 * @param num The number of bytes to compare
 * @return 0 if both memory regions are equal
 *         A positive value if the first differing byte in ptr1 is greater than the corresponding byte in ptr2
 *         A negative value if the first differing byte in ptr1 is less than the corresponding byte in ptr2
 */
int memcmp(const void* ptr1, const void* ptr2, size_t num);

/**
 *  ipv4_to_str:
 *  Converts an IPv4 address from a 32-bit integer (little endian) to a string representation
 * @param ip_addr The IPv4 address as a 32-bit integer
 * @param out_buffer A string buffer in which the string will be written (must be at least 16 bytes) 
 */
void ipv4_to_str(uint32_t ip_addr, unsigned char* out_buffer);

/**
 * ipv4_to_uint32:
 * converts an ipv4 address string (format "a.b.c.d") into a uint32_t 
 * @param str The address string
 * @return The converted address
 */
uint32_t ipv4_to_uint32(unsigned char* str);


/**
 * mac_to_str:
 * Writes a MAC address into a buffer in a human readable format
 * @param mac_addr A pointer to the MAC address (6 bytes)
 * @param out_buffer The string buffer in which to write (must be at least 18 bytes)
 */
void mac_to_str(uint8_t* mac_addr, unsigned char* out_buffer);

uint64_t min(uint64_t a, uint64_t b);
uint64_t max(int64_t a, int64_t b);

/**
 * ascii_to_uint32:
 * Converts an ASCII string representation of a number into an uint32_t
 * @param str The string
 * @return The number
 */
uint32_t ascii_to_uint32(unsigned char* str);
#endif