
#ifndef INCLUDE_USER_PROCESS_H
#define INCLUDE_USER_PROCESS_H

#include "../filesystem/vfs/vfs.h"
#include "../vector.h"
#include "../memory/memory.h"
#define MAX_FDS 512

#define FD_STDIN 0x0
#define FD_STDOUT 0x1
#define FD_STDERR 0x2

typedef struct {
    uint32_t process_id;
    uint32_t* page_dir;
    uint32_t kernel_stack;
    unsigned char* process_name;
    uint8_t running;
    uint32_t page_alloc_start;
    uint8_t priv_lvl;
    virt_mem_area_t* vm_areas;
    generic_file_t* fd_table[MAX_FDS];
} __attribute__((packed)) user_process_t;

#define USER_CODE_DATA_VMEMORY_START 0x00000000
#define USER_STACK_VMEMORY_START     0xbffffffb

#define USER_STACK_PAGES_PER_PROCESS 5

extern vector_t user_process_vector;

#define PRIV_STD 2
#define PRIV_SPECIAL 1
#define PRIV_ALUCARD 0

#define MAX_PIDS 32768
/**
 * create_user_process:
 * Allocates the needed memory for a user process and loads the binary into memory
 * 
 * @param binary A pointer to the first byte of the binary to be loaded into memory
 * @param size The size of the binary in bytes
 * @param process_name The name of the process
 * @param priv_lvl The priviledge level of the user process (0 highest)
 * @param argv An array of null terminated strings with the last index of the array being null
 * @return The proccess ID of the created process
 */
uint32_t create_user_process(unsigned char* binary, uint32_t size,unsigned char* process_name,uint8_t priv_lvl,unsigned char* argv[]);
/**
 * init_user_process_vector:
 * Sets up the user process vector
 */
void init_user_process_vector();
/**
 * kill_user_process:
 * Neutralizes the user process
 * @return Returns how successful the killing was
 */
int kill_user_process(uint32_t pid);

/**
 * get_user_process_by_pid:
 * Returns a user_process_t struct pointer for the process with the given pid
 * @param pid The process ID
 * 
 * @return The user process struct
 */
user_process_t* get_user_process_by_pid(uint32_t pid);


/**
 * dispatch_user_process:
 * Enters the user process if it is not already running
 * @param pid The process id
 */
void dispatch_user_process(uint32_t pid);

int assign_fd(user_process_t* proc,generic_file_t* file);

void free_fd(user_process_t* proc, generic_file_t* file);

user_process_t* get_current_user_process();

int get_pid();

void free_pid(uint32_t pid);

/**
 * run:
 * Executes a user process
 * @param filepath The path to the executable
 * @param argv An array of null terminated strings with the last index of the array being null
 * @param priv_lvl The priviledge level of the executable (0 highest priviledge level)
 */
void run(char* filepath,unsigned char* argv[],uint8_t priv_lvl);


void force_current_user_proc_as_kernel();

void restore_current_user_proc_from_kernel();
#endif