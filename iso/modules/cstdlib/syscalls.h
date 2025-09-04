#ifndef INCLUDE_SYSCALLS_H
#define INCLUDE_SYSCALLS_H

#include "fs.h"

#define FD_STDIN 0x0
#define FD_STDOUT 0x1
#define FD_STDERR 0x2

typedef struct {
    unsigned char* stdin_filename;
    unsigned char* stdout_filename;
    unsigned char* stderr_filename;
}process_fds_init_t;

#include "../../../src/tables/syscall_defines.h"
#include <stdint.h>
/**
 * write:
 * Writes a buffer to a file
 * @param fd The file descriptor to the file
 * @param buffer The buffer to write
 * @param count The length of the buffer
 * 
 * @return The number of bytes written
 */
int write(uint32_t fd, const char* buffer, uint32_t count);

/**
 * read:
 * Reads data from a file into a buffer
 * @param fd The file descriptor to the file
 * @param buffer The buffer to read into
 * @param count The maximum number of bytes to read
 * 
 * @return The number of bytes read
 */
int read(uint32_t fd, const char* buffer, uint32_t count);

/**
 * open:
 * Opens a file
 * @param pathname The path to the file
 * @param flags Flags for opening the file
 * 
 * @return The file descriptor for the opened file
 */
int open(const char* pathname, uint8_t flags);

/**
 * close:
 * Closes a file descriptor
 * @param fd The file descriptor to close
 * 
 * @return 0 on success, or -1 on error
 */
int close(uint32_t fd);

/**
 * exit:
 * Exits the current process
 * @param error_code The exit code
 * 
 * @return Does not return
 */
int exit(uint32_t error_code);

/**
 * mmap:
 * memory region into the current process
 * 
 * @param size The size of the memory region to allocate (Will be rounded up to the next page border)
 * @param prot The protections of the page
 * @param flags The page flags
 * @param fd The file descriptor if the mapping is backed by a file
 * @param offset The offset into the file
 * 
 * @return Pointer to the memory region
 */
void* mmap(uint32_t size, uint32_t prot, uint32_t flags, uint32_t fd, uint32_t offset);

/**
 * getcwd:
 * Writes a null terminated string of the current working directory to a buffer
 * 
 * @param buffer The buffer in which to write the string
 * @param size The size of the buffer
 * 
 */
void getcwd(unsigned char* buffer, uint32_t size);

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
int getdents(uint32_t fd,dirent_t* buffer,uint32_t size);

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


/**
 * seek:
 * Sets the read-write pointer of a file
 * @param fd The file descriptor to the file
 * @param offset The new offset of the rw pointer
 * 
 * @return The offset (may be capped at the filesize)
 */
int seek(uint32_t fd,uint32_t offset);

/**
 * ioctl:
 * Sends a device specific command to a device file
 * @param fd The file descriptor to the device file
 * @param cmd The command to send
 * @param arg An argument for the command
 * 
 * @return SYSCALL_FAIL if something failed, 
 * 
 *  else SYSCALL_SUCCESS
 */
int ioctl(uint32_t fd, uint32_t cmd,void* arg);

/**
 * mssleep:
 * Sends the current process to sleep for at least the specified time
 * @param time The number of milliseconds to go to sleep
 * @return SYSCALL_SUCCESS
 */
int mssleep(uint32_t time);

/**
 * spawn:
 * Spawns a new process, which is completely independant of the spawning process
 * @param filename The name of the file, which contains the binary for the process
 * @param argv The argument vector for the new process, can either be 0 for no arguments or an array of null-terminated strings which the last entry is a null-entry
 *      e. g. unsigned char* argv[] = {"arg_one", "arg_two",...,0}
 * @param start_fds The stdin / stdout / stderr filenames (may be null which causes the file descriptor / all file descriptors to be /dev/null)
 * @return SYSCALL_FAIL if something failed, 
 * 
 *  else SYSCALL_SUCCESS
 */
int spawn(unsigned char* filename, unsigned char* argv[],process_fds_init_t* start_fds);

/**
 * mknod:
 * Creates a file system node (currently just for making pipes)
 * @param filename The name of the newly created file
 * @param type currently just FS_TYPE_PIPE
 * 
 * @return SYSCALL_FAIL if something failed, 
 * 
 *  else SYSCALL_SUCCESS
 */
int mknod(unsigned char* filename, uint32_t type);
/**
 * getpid:
 * Returns the process id of the calling process
 * @return The pid
 */
int getpid();
#endif