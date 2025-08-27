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
#include "../tables/syscalls.h"
#include "../filesystem/vfs/vfs.h"
#include "../drivers/keyboard/keyboard.h"
#include "../screen/screen.h"
#include "../filesystem/filesystem.h"
#include "../filesystem/file_operations.h"
#include "../kernel_header.h"
#include "../tables/syscall_defines.h"
vector_t user_process_vector;
static uint8_t pid_used[MAX_PIDS] = {0};
static uint32_t next_pid = 1;


user_process_t* get_user_process_by_pid(uint32_t pid){
    for (uint32_t i = 0; i < user_process_vector.size;i++){
        if (((user_process_t*)(user_process_vector.data[i]))->process_id == pid){
            return (user_process_t*)(user_process_vector.data[i]);
        }
    }
    return 0;
}

user_process_t* get_current_user_process(){
    if (!dispatched_user_mode) return &global_kernel_process; // useful for when I want to use syscalls internally

    process_state_t* state = get_current_process_state();

    uint32_t curr_pid = state->pid;

    return get_user_process_by_pid(curr_pid);

}

int get_pid(){
    for (uint32_t i = 0; i < MAX_PIDS; ++i) {
        uint32_t pid = next_pid;
        next_pid = (next_pid % MAX_PIDS) + 1;
        if (!pid_used[pid]) {
            pid_used[pid] = 1;
            return pid;
        }
    }
    return -1;
}

int assign_fd(user_process_t* proc,generic_file_t* file){
    for (int i = 3; i < MAX_FDS; ++i) {
        if (proc->fd_table[i] == 0) {
            proc->fd_table[i] = file;
            return i;
        }
    }
    return -1; 
}

void free_fd(user_process_t* proc, generic_file_t* file){
    for (int i = 3; i < MAX_FDS; ++i) {
        if (proc->fd_table[i] == file) {
            proc->fd_table[i] = 0;
        }
    }
}

void free_pid(uint32_t pid){
    if (pid > 0 && pid < MAX_PIDS){
        pid_used[pid] = 0;
    }
}


uint32_t create_user_process(unsigned char* binary, uint32_t size,unsigned char* process_name,uint8_t priv_lvl) {
    uint32_t int_save = get_interrupt_status();
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
    uint32_t kernel_stack = (uint32_t)kmalloc(MEMORY_PAGE_SIZE);

    user_process_t* process = (user_process_t*)kmalloc(sizeof(user_process_t));
    memset(process->fd_table,0,MAX_FDS);
    process->fd_table[FD_STDIN] = &kb_file; 
    process->fd_table[FD_STDOUT] = &screen_file;
    process->vm_areas = nullptr;
    process->running = 0;
    process->priv_lvl = priv_lvl;
    process->kernel_stack = kernel_stack;
    uint32_t name_len = strlen(process_name);
    process->process_name = (unsigned char*)kmalloc(name_len + 1);
    memcpy(process->process_name,process_name,name_len + 1);

    int pid = get_pid();
    if (pid == -1) return 0;
    process->process_id = pid;

    uint32_t code_data_pages = CEIL_DIV(size,MEMORY_PAGE_SIZE);
    process->page_alloc_start = code_data_pages * MEMORY_PAGE_SIZE;

    vector_append(&user_process_vector,(uint32_t)process); // too lazy to implement a vector for structs
    
    uint32_t* pd = create_user_page_dir();
    
    process->page_dir = pd;
    add_process_state(process);
    
    for (uint32_t i = 0; i < code_data_pages;i++){
        uint32_t code_data_mem = pmm_alloc_page_frame();
        // some weird kernel mapping stuff because if I change the current page directory too early the OS shits itself
        mem_map_page(TEMP_KERNEL_COPY_ADDR,code_data_mem, PAGE_FLAG_WRITE | PAGE_FLAG_PRESENT);
        
        // copy binary into mapped page
        uint32_t offset = i * MEMORY_PAGE_SIZE;
        uint32_t to_copy = (size - offset > MEMORY_PAGE_SIZE) ? MEMORY_PAGE_SIZE : (size - offset);
        if (to_copy > 0){
            void* dest = (void*)(TEMP_KERNEL_COPY_ADDR);
            memcpy(dest,binary + offset,to_copy);
        }

        mem_unmap_page(TEMP_KERNEL_COPY_ADDR);
        mem_map_page_in_dir(process->page_dir,USER_CODE_DATA_VMEMORY_START + i * MEMORY_PAGE_SIZE,code_data_mem,PAGE_FLAG_WRITE | PAGE_FLAG_USER);
    }
    for (uint32_t i = 0; i < USER_STACK_PAGES_PER_PROCESS;i++){
        uint32_t stack_mem = pmm_alloc_page_frame();
        mem_map_page_in_dir(process->page_dir,USER_STACK_VMEMORY_START - (i * MEMORY_PAGE_SIZE),stack_mem,PAGE_FLAG_WRITE | PAGE_FLAG_USER);
    }
    
    set_interrupt_status(int_save);

    return process->process_id;

}

void load_registers(){
    uint32_t int_save = get_interrupt_status();
    disable_interrupts();
    // we do a little pretending here so that when the scheduler returns with these values, everything starts
    process_state_t* state = get_process_state_by_page_dir(mem_get_current_page_dir());
    const uint32_t user_mode_data_segment_selector = (0x20 | 0x3);
    
    state->regs.ds = user_mode_data_segment_selector;
    state->regs.es = user_mode_data_segment_selector;
    state->regs.fs = user_mode_data_segment_selector;
    state->regs.gs = user_mode_data_segment_selector;

    state->regs.cs = (0x18 | 0x3);
    state->regs.eflags = (1 << 9) | (3 << 12); // enable interrupts for user
    state->regs.eip = USER_CODE_DATA_VMEMORY_START;
    state->regs.esp = USER_STACK_VMEMORY_START;
    state->regs.ebp = USER_STACK_VMEMORY_START;
    state->regs.ss = user_mode_data_segment_selector;
    set_interrupt_status(int_save);
}

void dispatch_user_process(uint32_t pid){
    user_process_t* process = get_user_process_by_pid(pid);
    if (process->running) return;
    process->running = 1;
    mem_change_page_dir(process->page_dir); 
    set_kernel_stack(process->kernel_stack + MEMORY_PAGE_SIZE);
    load_registers();
}


void init_user_process_vector(){
    init_vector(&user_process_vector);
}

int kill_user_process(uint32_t pid){
    if (!(pid > 0 && pid < MAX_PIDS && pid_used[pid])) return SYSCALL_FAIL;
    user_process_t* process;
    for (uint32_t i = 0; i < user_process_vector.size;i++){
        if (((user_process_t*)(user_process_vector.data[i]))->process_id == pid){
            process = (user_process_t*)(user_process_vector.data[i]);
            vector_erase(&user_process_vector,i);
            break;
        }
    }
    
    virt_mem_area_t* vma = process->vm_areas;
    virt_mem_area_t* prev_vma;
    while(vma){
        
        if (vma->shrd_obj){
            for (unsigned int i = 0; i < vma->shrd_obj->n_pages;i++){
                shared_page_t* shrd_page = vma->shrd_obj->shared_pages[i];
                if (!shrd_page) continue;

                shrd_page->ref_count--;
                if (shrd_page->ref_count == 0){
                    kfree(shrd_page);
                    vma->shrd_obj->shared_pages[i] = nullptr;
                }
            }

            vma->shrd_obj->ref_count--;

            if (vma->shrd_obj->ref_count == 0){
                kfree(vma->shrd_obj);
            }
            
        }
        prev_vma = vma;
        vma = vma->next;
        kfree(prev_vma);
    }

    process_state_t* proc_state = get_process_state_by_page_dir(process->page_dir);
    if (!proc_state) {error("Getting process state failed"); return SYSCALL_FAIL;};
    remove_process_state(proc_state);
    free_user_page_dir(process->page_dir);
    kfree(process->process_name);
    kfree((void*)process->kernel_stack);
    kfree(process);
    free_pid(pid);

    return 0;
}

void run(char* filepath,uint8_t priv_lvl){
    inode_t* file = get_inode_by_full_file_path(filepath);

    if (!file){
        error("Fetching file failed");
        return;
    }

    unsigned char* binary = (unsigned char*)kmalloc(file->size);
    int fd = sys_open(&global_kernel_process,filepath,FILE_FLAG_READ);

    if (fd < 0){
        error("FD failed");
        return;
    }

    int ret_val = sys_read(&global_kernel_process,fd,binary,file->size);
    if (ret_val < 0){
        panic("Failed to read executable file");
        return;
    }
    uint8_t old_int_status = get_interrupt_status();
    disable_interrupts();

    uint32_t pid = create_user_process(binary,file->size,filepath,priv_lvl);

    if (!pid) {
        error("Creating user process failed");
        return;
    }

    dispatch_user_process(pid);
    set_interrupt_status(old_int_status);

}