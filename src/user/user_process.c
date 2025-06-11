#include "user_process.h"
#include "../memory/memory.h"
#include "../memory/kmalloc.h"
#include "../util.h"
#include "../multiboot.h"
#include "../io/log.h"

vector_t user_process_vector;
static unsigned char pid_used[MAX_PIDS] = {0};
static unsigned int next_pid = 1;

unsigned int get_pid(){
    for (unsigned int i = 0; i < MAX_PIDS; ++i) {
        unsigned int pid = next_pid;
        next_pid = (next_pid % MAX_PIDS) + 1;
        if (!pid_used[pid]) {
            pid_used[pid] = 1;
            return pid;
        }
    }
    return 0;
}

unsigned int free_pid(unsigned int pid){
    if (pid > 0 && pid < MAX_PIDS){
        pid_used[pid] = 0;
    }
}


unsigned int create_user_process(unsigned char* binary, unsigned int size) {
// To setup a user process:
  /**
   * create_user_page_dir()
   * mem_change_page_dir()
   * pmm_alloc_frame_page() for code/data and stack
   * load programs into page frames
   * mem_map_page() for both pages (code/data at 0x00000000, stack at 0xBFFFFFFB) with flag PAGE_FLAG_USER
   */


    // All kmalloc calls need to be made before creating the page directory, otherwise they are not correctly copied 

    //allocate a kernel stack
    unsigned int kernel_stack = (unsigned int)kmalloc(MEMORY_PAGE_SIZE);

    user_process_t* process = (user_process_t*)kmalloc(sizeof(user_process_t));
    process->kernel_stack = kernel_stack;


    vector_append(&user_process_vector,(unsigned int)process); // too lazy to implement a vector for structs

    unsigned int* pd = create_user_page_dir();
    process->page_dir = pd;
    
    
    unsigned int code_data_pages = CEIL_DIV(size,MEMORY_PAGE_SIZE);
    for (unsigned int i = 0; i < code_data_pages;i++){
        unsigned int code_data_mem = pmm_alloc_page_frame();
        // some weird kernel mapping stuff because if I change the current page directory too early the OS shits itself
        mem_map_page(TEMP_KERNEL_COPY_ADDR,code_data_mem, PAGE_FLAG_WRITE | PAGE_FLAG_PRESENT);
        
        // copy binary into mapped page
        unsigned int offset = i * MEMORY_PAGE_SIZE;
        unsigned int to_copy = (size - offset > MEMORY_PAGE_SIZE) ? MEMORY_PAGE_SIZE : (size - offset);
        if (to_copy > 0){
            void* dest = (void*)(TEMP_KERNEL_COPY_ADDR);
            memcpy(dest,binary + offset,to_copy);
        }

        mem_unmap_page(TEMP_KERNEL_COPY_ADDR);
        mem_map_page_in_dir(process->page_dir,USER_CODE_DATA_VMEMORY_START + i * MEMORY_PAGE_SIZE,code_data_mem,PAGE_FLAG_WRITE | PAGE_FLAG_USER);
    }

    unsigned int stack_mem = pmm_alloc_page_frame();
    mem_map_page_in_dir(process->page_dir,USER_STACK_VMEMORY_START,stack_mem,PAGE_FLAG_WRITE | PAGE_FLAG_USER);
    
    
    unsigned int pid = get_pid();
    if (pid == 0) return 0;
    process->process_id = pid;
    
    
    
    mem_change_page_dir(process->page_dir); 
    

    return process->process_id;
}

user_process_t* get_user_process_by_pid(unsigned int pid){
    for (unsigned i = 0; i < user_process_vector.size;i++){
        if (((user_process_t*)(user_process_vector.data[i]))->process_id == pid){
            return (user_process_t*)(user_process_vector.data[i]);
        }
    }
    return 0;
}

void init_user_process_vector(){
    init_vector(&user_process_vector);
}

void kill_user_process(unsigned int pid){
    if (!(pid > 0 && pid < MAX_PIDS && pid_used[pid])) return;
    user_process_t* process;
    for (unsigned i = 0; i < user_process_vector.size;i++){
        if (((user_process_t*)(user_process_vector.data[i]))->process_id == pid){
            process = (user_process_t*)(user_process_vector.data[i]);
            vector_erase(&user_process_vector,i);
            break;
        }
    }
    free_user_page_dir(process->page_dir);
    kfree((void*)process->kernel_stack);
    kfree(process);
    free_pid(pid);
}