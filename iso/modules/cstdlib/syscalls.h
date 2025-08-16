#ifndef INCLUDE_SYSCALLS_H
#define INCLUDE_SYSCALLS_H

#include "fs.h"

#define FD_STDIN 0x0
#define FD_STDOUT 0x1
#define FD_STDERR 0x2
#include "../../../src/tables/syscall_defines.h"

/**
 * write:
 * Writes a buffer to a file
 * @param fd The file descriptor to the file
 * @param buffer The buffer to write
 * @param count The length of the buffer
 * 
 * @return The number of bytes written
 */
int write(unsigned int fd, const char* buffer, unsigned int count);

/**
 * read:
 * Reads data from a file into a buffer
 * @param fd The file descriptor to the file
 * @param buffer The buffer to read into
 * @param count The maximum number of bytes to read
 * 
 * @return The number of bytes read
 */
int read(unsigned int fd, const char* buffer, unsigned int count);

/**
 * open:
 * Opens a file
 * @param pathname The path to the file
 * @param flags Flags for opening the file
 * 
 * @return The file descriptor for the opened file
 */
int open(const char* pathname, unsigned char flags);

/**
 * close:
 * Closes a file descriptor
 * @param fd The file descriptor to close
 * 
 * @return 0 on success, or -1 on error
 */
int close(unsigned int fd);

/**
 * exit:
 * Exits the current process
 * @param error_code The exit code
 * 
 * @return Does not return
 */
int exit(unsigned int error_code);

/**
 * mmap:
 * Allocates and maps a memory region into the current process
 * 
 * @param size The size of the memory region to allocate (Will be rounded up to the next page border)
 * 
 * @return Pointer to the allocated memory region
 */
void* mmap(unsigned int size);

/**
 * getcwd:
 * Writes a null terminated string of the current working directory to a buffer
 * 
 * @param buffer The buffer in which to write the string
 * @param buf_len The length of the buffer
 * 
 */
void getcwd(unsigned char* buffer, unsigned int buf_len);

/**
 * getdents:
 * Reads the entries of a directory to a buffer of dirent_t (see fs.h) structs
 *
 * @param fd The fd of the directory
 * @param buffer The buffer to write into
 * @param size The size of the buffer
 * 
 * @return The number of directory entries read 
 */
int getdents(unsigned int fd,dirent_t* buffer,unsigned int size);

/**
 * chdir:
 * Changes the currently active directory
 * 
 * @param dir_name The full or relative path of the new directory
 * 
 * @return SYSCALL_FAIL if something failed, 
 * 
 *  else SYSCALL_SUCCESS
 */
int chdir(unsigned char* dir_name);

/**
 * rmfile:
 * Deletes a file or empty directory
 * @param filename The full or relative path to the file
 * @return SYSCALL_FAIL if something failed, 
 * 
 *  else SYSCALL_SUCCESS
 */
int rmfile(unsigned char* filename);
#endif