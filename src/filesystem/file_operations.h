
#ifndef INCLUDE_FILE_OPS
#define INCLUDE_FILE_OPS

typedef struct {
    unsigned char flags;
    unsigned int fd;
    unsigned int inode_id
}open_file_t;

#define MAX_FDS 32768
#define FILE_FLAG_READ 0x1
#define FILE_FLAG_WRITE 0x2
#define FILE_FLAG_EXEC 0x4

#define FILE_OP_FAILED -1
#define FILE_OP_SUCCESS 0
#define FILE_INVALID_FD -1

extern vector_t fd_vector;

/**
 * open:
 * Opens a file and returns a file descriptor to it
 * @param filepath The path to the file
 * @param flags flags for the file (R/W/X)
 * @return The file desciptor
 */
int open(unsigned char* filepath,unsigned char flags);


/**
 * write:
 * Writes to a file descriptor
 * @param fd The file descriptor to write to
 * @param buffer The buffer to write into the fd
 * @param size The number of bytes to write
 * @return The number of bytes written
 */
int write(unsigned int fd, unsigned char* buffer,unsigned int size);

/**
 * close: 
 * Closes a file descriptor
 * @param fd The filde descriptor to close
 * @return 0 if successful
 * 
 *         -1 if invalid fd
 */
int close(unsigned int fd);

/**
 * read: 
 * Reads from a file descriptor into a buffer
 * @param fd The file descriptor
 * @param buffer The buffer to read into
 * @param size The size to read
 * 
 * @return The number of bytes read
 */
int read(unsigned int fd, unsigned char* buffer, unsigned int size);

#endif