
#ifndef INCLUDE_USER_PROCESS_H
#define INCLUDE_USER_PROCESS_H

#include "../filesystem/vfs/vfs.h"

#define MAX_FDS 512

typedef struct {
    unsigned int process_id;
    unsigned int* page_dir;
    unsigned int kernel_stack;
    unsigned char* process_name;
    unsigned char running;
    generic_file_t* fd_table[MAX_FDS];
} __attribute__((packed)) user_process_t;

#define USER_CODE_DATA_VMEMORY_START 0x00000000
#define USER_STACK_VMEMORY_START     0xbffffffb

#define MAX_PIDS 32768
/**
 * create_user_process:
 * Allocates the needed memory for a user process and loads the binary into memory
 * 
 * @param binary A pointer to the first byte of the binary to be loaded into memory
 * @param size The size of the binary in bytes
 * @param process_name The name of the process
 * @return The proccess ID of the created process
 */
unsigned int create_user_process(unsigned char* binary, unsigned int size,unsigned char* process_name);
/**
 * init_user_process_vector:
 * Sets up the user process vector
 */
void init_user_process_vector();
/**
 * kill_user_process:
 * Neutralizes the user process
 */
void kill_user_process(unsigned int pid);

void enter_user_mode();

/**
 * get_user_process_by_pid:
 * Returns a user_process_t struct pointer for the process with the given pid
 * @param pid The process ID
 * 
 * @return The user process struct
 */
user_process_t* get_user_process_by_pid(unsigned int pid);


/**
 * dispatch_user_process:
 * Enters the user process if it is not already running
 * @param pid The process id
 */
void dispatch_user_process(unsigned int pid);

int assign_fd(user_process_t* proc,generic_file_t* file);

void free_fd(user_process_t* proc, generic_file_t* file);

#endif