
#ifndef INCLUDE_FSUTIL_H
#define INCLUDE_FSUTIL_H

#include <shared/util.h>

/**
 * get_full_active_path:
 * Returns a null terminated string with the current active directory
 * 
 * @param path_buffer The buffer in which to write the string
 * @param buf_len The length of the buffer
 * @return The current working directory
 * 
 */
int get_full_active_path(unsigned char* path_buffer, uint32_t buf_len);


/**
 * split_filepath:
 * Splits a filepath into preceding directories and last filename
 * 
 * @param path The path to split
 * 
 * @return A string array of either one or two strings, depending on if there are preceding directories
 */
string_array_t* split_filepath(unsigned char* path);

/**
 * Frees a string array assuming the array, the strings and the char* are heap allocated
 * @param str_arr The string array
 */
void free_string_arr(string_array_t* str_arr);
#endif