
#ifndef INCLUDE_FSUTIL_H
#define INCLUDE_FSUTIL_H

#include "../util.h"

/**
 * get_full_active_path:
 * Returns a null terminated string with the current active directory
 * 
 * @param path_buffer The buffer in which to write the string
 * @param buf_len The length of the buffer
 * @return The current working directory
 * 
 */
int get_full_active_path(unsigned char* path_buffer, unsigned int buf_len);

#endif