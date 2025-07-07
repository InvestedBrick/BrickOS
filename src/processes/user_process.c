#include "user_process.h"
#include "../memory/memory.h"
#include "../memory/kmalloc.h"
#include "../vector.h"
#include "../util.h"
#include "../multiboot.h"
#include "../io/log.h"
#include "scheduler.h"
#include "../tables/tss.h"
#include "../tables/interrupts.h"

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

void free_pid(unsigned int pid){
    if (pid > 0 && pid < MAX_PIDS){
        pid_used[pid] = 0;
    }
}


unsigned int create_user_process(unsigned char* binary, unsigned int size,unsigned char* process_name) {
    unsigned int int_save = get_interrupt_status();
    disable_interrupts();
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
    process->running = 0;
    process->kernel_stack = kernel_stack;
    unsigned int name_len = strlen(process_name);
    process->process_name = (unsigned char*)kmalloc(name_len + 1);
    memcpy(process->process_name,process_name,name_len + 1);

    vector_append(&user_process_vector,(unsigned int)process); // too lazy to implement a vector for structs

    unsigned int* pd = create_user_page_dir();
    
    process->page_dir = pd;
    add_process_state(pd);
    
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
    

    return process->process_id;

    set_interrupt_status(int_save);
}

void load_registers(){
    unsigned int int_save = get_interrupt_status();
    disable_interrupts();
    // we do a little pretending here so that when the scheduler returns with these values, everything starts
    process_state_t* state = get_process_state_by_page_dir(mem_get_current_page_dir());
    const unsigned int user_mode_data_segment_selector = (0x20 | 0x3);
    
    state->regs.ds = user_mode_data_segment_selector;
    state->regs.es = user_mode_data_segment_selector;
    state->regs.fs = user_mode_data_segment_selector;
    state->regs.gs = user_mode_data_segment_selector;

    state->regs.cs = (0x18 | 0x3);
    state->regs.eflags = (1 << 9) | (3 << 12); // enable interrupts for user
    state->regs.eip = USER_CODE_DATA_VMEMORY_START;
    state->regs.esp = USER_STACK_VMEMORY_START;
    state->regs.ss = user_mode_data_segment_selector;
    set_interrupt_status(int_save);
}

void dispatch_user_process(unsigned int pid){
    user_process_t* process = get_user_process_by_pid(pid);
    if (process->running) return;
    process->running = 1;
    mem_change_page_dir(process->page_dir); 
    set_kernel_stack(process->kernel_stack + MEMORY_PAGE_SIZE);
    //enter_user_mode();
    load_registers();
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
    process_state_t* proc_state = get_process_state_by_page_dir(process->page_dir);
    if (!proc_state) panic("Getting process state failed");
    remove_process_state(proc_state);
    free_user_page_dir(process->page_dir);
    kfree(process->process_name);
    kfree((void*)process->kernel_stack);
    kfree(process);
    free_pid(pid);
}