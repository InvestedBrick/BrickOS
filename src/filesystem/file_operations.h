
#ifndef INCLUDE_FILE_OPS
#define INCLUDE_FILE_OPS

#include "vfs/vfs.h"
typedef struct {
    unsigned char flags;
    unsigned int inode_id;
    unsigned int rw_pointer; //TODO: implement lseek and get_file_size 
}open_file_t;


#define FILE_READ_ALL (unsigned int)-1
#define MAX_FILE_SECTORS (NUM_DATA_SECTORS_PER_FILE + ATA_SECTOR_SIZE / sizeof(unsigned int))

#define nullptr 0
/**
 * fs_open:
 * Opens a file and returns a file descriptor to it
 * @param filepath The path to the file
 * @param flags flags for the file (R/W/A/C)
 * @return The file desciptor
 */
generic_file_t* fs_open(unsigned char* filepath,unsigned char flags);


/**
 * fs_write:
 * Writes to a file descriptor
 * @param fd The file descriptor to write to
 * @param buffer The buffer to write into the fd
 * @param size The number of bytes to write
 * @return The number of bytes written
 */
int fs_write(generic_file_t* file, unsigned char* buffer,unsigned int size);

/**
 * fs_close: 
 * Closes a file descriptor
 * @param fd The filde descriptor to close
 * @return 0 if successful
 * 
 *         -1 if invalid fd
 */
int fs_close(generic_file_t* file);

/**
 * fs_read: 
 * Reads from a file descriptor into a buffer
 * @param fd The file descriptor
 * @param buffer The buffer to read into
 * @param size The size to read
 * 
 * @return The number of bytes read
 */
int fs_read(generic_file_t* file, unsigned char* buffer, unsigned int size);


#endif