
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
 * streq:
 * Compares the length of two strings
 * @param str1 A pointer to the first string
 * @param str2 A pointer to the second string
 * @param length The max length of both strings
 * 
 * @return 1 if both strings are equal
 *         0 else 
 */
char streq(const char* str1, const char* str2, unsigned int length);

/**
 * strlen: 
 * Returns the length of a null-terminated string
 * @param str The string
 * @return The length of the string
 */
unsigned int strlen(unsigned char* str);

typedef struct {
    unsigned int size;
    unsigned int capacity;
    unsigned int* data;
}vector_t;

/**
 * vector_append:
 * Appends data to a vector
 * @param vec The vecror
 * @param data The data to be appended
 */
void vector_append(vector_t* vec, unsigned int data);

/**
 * vector_pop:
 * Pops off and returns the top element of a vector
 * 
 * @param vec The vector
 * @return The popped item
 */
unsigned int vector_pop(vector_t* vec);

/**
 * vector_is_empty:
 * Returns whether a vector is empty
 * @return 1 if the vector is empty
 * 
 *         0 if the vector is not empty
 */
unsigned char vector_is_empty(vector_t* vec);

/**
 * init_vector:
 * Initializes a vector
 * @param vec The vector to be initialized
 */
void init_vector(vector_t* vec);
/**
 * vector_erase:
 * Erases an item at a given index in a vector
 * @return The erased item
 */
unsigned int vector_erase(vector_t* vec,unsigned int idx);

/**
 * vector_free:
 * Frees the data of a vector and if given the data itself (if the data was pointers)
 * @param vec The vector
 * @param ptrs Whether the vector data is pointers that need to be freed
 */
void vector_free(vector_t* vec, unsigned char ptrs);

/**
 * Frees a string array assuming the array, the strings and the char* are heap allocated
 * @param str_arr The string array
 */
void free_string_arr(string_array_t* str_arr);
#endif