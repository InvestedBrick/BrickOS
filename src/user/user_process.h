#pragma once

#ifndef USER_LAND_H
#define USER_LAND_H

typedef struct {
    unsigned int process_id;
    unsigned int* page_dir;
    unsigned int kernel_stack;
} __attribute__((packed)) user_process_t;

#define USER_CODE_DATA_VMEMORY_START 0x00000000
#define USER_STACK_VMEMORY_START     0xbffffffb

#define MAX_PIDS 32768
/**
 * create_user_process:
 * Allocates the needed memory for a user process, loads the binary into memory and sets the page directory to a user page dir
 * 
 * @param binary A pointer to the first byte of the binary to be loaded into memory
 * @param size The size of the binary in bytes
 * @return The proccess ID of the created process
 */
unsigned int create_user_process(unsigned char* binary, unsigned int size);
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
#endif