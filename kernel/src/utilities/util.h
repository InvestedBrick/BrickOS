
#ifndef INCLUDE_UTIL_H
#define INCLUDE_UTIL_H

#define CEIL_DIV(a,b) (((a + b) - 1 )/ b)
#define ALIGN_DOWN(a, b) ((b) ? ((a) - ((a) % (b))) : (a))
#define ALIGN_UP(a, b) (((a) + (b) - 1) & ~((b) - 1))
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "vector.h"
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
 * @note This struct is used for when a memory page has been mapped multiple times by uACPI and therefore might unmap too much
 * to prevent that, use the shared_address_add and shared_address_remove functions to manage these pages
 */
typedef struct {
    void* addr;
    uint32_t cntr;
}shared_addr_t; 

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

/**
 * shared_address_add:
 * Adds a new shared address to a vector or increments its counter if it already exists
 * @param vec The vector of shared addresses
 * @param addr The address
 * @return A boolean that is true if the address has just been added and false if only its counter got incremented
 */
bool shared_address_add(vector_t* vec,void* addr);

/**
 * shared_address_remove:
 * Decrements the counter of a shared address and frees it if that was the last holder of the address
 * @param vec The vector of shared addresses
 * @param addr The address
 * @return A boolean that is true if the address was freed and false if only its counter was decreased
 */
bool shared_address_remove(vector_t* vec, void* addr);

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
 * @return A pointer to a null-terminated string representing the IPv4 address in dotted-decimal notation 
 */
unsigned char* ipv4_to_str(uint32_t ip_addr);


/**
 * log_MAC:
 * Logs the MAC address in a human-readable format
 * @param mac_addr A pointer to the MAC address (6 bytes)
 */
void log_MAC(uint8_t* mac_addr);

uint64_t min(uint64_t a, uint64_t b);
uint64_t max(int64_t a, int64_t b);
#endif