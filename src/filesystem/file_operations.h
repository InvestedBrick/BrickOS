
#ifndef INCLUDE_FILE_OPS
#define INCLUDE_FILE_OPS

#include "vfs/vfs.h"
typedef struct {
    uint8_t flags;
    uint32_t inode_id;
    uint32_t rw_pointer;
}open_file_t;


#define FILE_READ_ALL (uint32_t)-1
#define MAX_FILE_SECTORS (NUM_DATA_SECTORS_PER_FILE + ATA_SECTOR_SIZE / sizeof(uint32_t))

#define nullptr 0
/**
 * fs_open:
 * Opens a file and returns a file descriptor to it
 * @param filepath The path to the file
 * @param flags flags for the file (R/W/A/C)
 * @return The file desciptor
 */
generic_file_t* fs_open(unsigned char* filepath,uint8_t flags);


/**
 * fs_write:
 * Writes to a file
 * @param file The file to write to
 * @param buffer The buffer to write into the fd
 * @param size The number of bytes to write
 * @return The number of bytes written
 */
int fs_write(generic_file_t* file, unsigned char* buffer,uint32_t size);

/**
 * fs_close: 
 * Closes a file
 * @param file The file to close
 * @return 0 if successful
 * 
 *         -1 if invalid fd
 */
int fs_close(generic_file_t* file);

/**
 * fs_read: 
 * Reads from a file into a buffer
 * @param file The file to read from
 * @param buffer The buffer to read into
 * @param size The size to read
 * 
 * @return The number of bytes read
 */
int fs_read(generic_file_t* file, unsigned char* buffer, uint32_t size);

/**
 * fs_seek:
 * Sets the read-write pointer of a file to the given offset
 * @param file The file to seek
 * @param offset The new offset
 * 
 * @return The offset which may be capped at the filesize
 */
int fs_seek(generic_file_t* file, uint32_t offset);
#endif