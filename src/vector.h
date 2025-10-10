
#ifndef INCLUDE_VECTOR_H
#define INCLUDE_VECTOR_H

#include <stdint.h>

typedef uint64_t vector_data_t;

typedef struct {
    uint32_t size;
    uint32_t capacity;
    vector_data_t* data;
}vector_t;

/**
 * vector_append:
 * Appends data to a vector
 * @param vec The vecror
 * @param data The data to be appended
 */
void vector_append(vector_t* vec, vector_data_t data);

/**
 * vector_pop:
 * Pops off and returns the top element of a vector
 * 
 * @param vec The vector
 * @return The popped item
 */
vector_data_t vector_pop(vector_t* vec);

/**
 * vector_is_empty:
 * Returns whether a vector is empty
 * @return 1 if the vector is empty
 * 
 *         0 if the vector is not empty
 */
uint8_t vector_is_empty(vector_t* vec);

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
vector_data_t vector_erase(vector_t* vec,uint32_t idx);
/** 
 * vector_find: 
 * Returns the index of a piece of data in the vector
 * @param vec The vector
 * @param data The data 
 * @return The index of the data in the vector
*/
uint32_t vector_find(vector_t* vec, vector_data_t data);

/**
 * vector_free:
 * Frees the data of a vector and if given the data itself (if the data was pointers)
 * @param vec The vector
 * @param ptrs Whether the vector data is pointers that need to be freed
 */
void vector_free(vector_t* vec, uint8_t ptrs);

/**
 * vector_erase_item:
 * Erases an item from a vector
 * @param vec The vector
 * @param data The item to erase
 * 
 * @return The erased item (or (vector_data_t)-1) if data was not found
 */
vector_data_t vector_erase_item(vector_t* vec, vector_data_t data);
#endif