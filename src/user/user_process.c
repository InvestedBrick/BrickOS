#include "user_process.h"
#include "../memory/memory.h"
#include "../memory/kmalloc.h"
#include "../util.h"

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

unsigned int create_user_process() {
// To setup a user process:
  /**
   * create_user_page_dir()
   * mem_change_page_dir()
   * pmm_alloc_frame_page() for code/data and stack
   * load programs into page frames
   * mem_map_page() for both pages (code/data at 0x00000000, stack at 0xBFFFFFFB) with flag PAGE_FLAG_USER
   */
    user_process_t* process = (user_process_t*)kmalloc(sizeof(user_process_t));
    unsigned int* pd = create_user_page_dir();
    mem_change_page_dir(pd);
    unsigned int code_data_mem = pmm_alloc_page_frame();
    unsigned int stack_mem = pmm_alloc_page_frame();

    mem_map_page(USER_CODE_DATA_VMEMORY_START,code_data_mem,PAGE_FLAG_WRITE | PAGE_FLAG_USER);
    mem_map_page(USER_STACK_VMEMORY_START,stack_mem,PAGE_FLAG_WRITE | PAGE_FLAG_USER);

    process->page_dir = pd;
    unsigned int pid = get_pid();
    if (pid == 0) return 0;
    process->process_id = pid;
    vector_append(&user_process_vector,(unsigned int)process); // too lazy to implement a vector for structs

    return process->process_id;
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
    kfree(process);
    free_pid(pid);
}