
#ifndef INCLUDE_VECTOR_H
#define INCLUDE_VECTOR_H
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
 * vector_find: 
 * Returns the index of a piece of data in the vector
 * @param vec The vector
 * @param data The data 
 * @return The index of the data in the vector
*/
unsigned int vector_find(vector_t* vec, unsigned int data);

/**
 * vector_free:
 * Frees the data of a vector and if given the data itself (if the data was pointers)
 * @param vec The vector
 * @param ptrs Whether the vector data is pointers that need to be freed
 */
void vector_free(vector_t* vec, unsigned char ptrs);

#endif