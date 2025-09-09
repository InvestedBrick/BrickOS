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
#include "../filesystem/devices/devs.h"
vector_t user_process_vector;
static uint8_t pid_used[MAX_PIDS] = {0};
static uint32_t next_pid = 1;
user_process_t* overwrite_proc = 0;

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

    if (overwrite_proc) return overwrite_proc;

    process_state_t* state = get_current_process_state();

    uint32_t curr_pid = state->pid;

    return get_user_process_by_pid(curr_pid);

}

// this is the stuff that is created when you just want stuff to work
void overwrite_current_proc(user_process_t* proc){
    overwrite_proc = proc;
}

void restore_active_proc(){
    overwrite_proc = nullptr;
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

void setup_arguments(user_process_t* proc,unsigned char* argv[]){
    uint32_t page_phys = pmm_alloc_page_frame();
    process_state_t* state = get_process_state_by_page_dir(proc->page_dir);
    // map temporarily to write memory into it
    mem_map_page(TEMP_KERNEL_COPY_ADDR,page_phys,PAGE_FLAG_WRITE | PAGE_FLAG_PRESENT);
    unsigned char* page = (unsigned char*)TEMP_KERNEL_COPY_ADDR;
    memset(page,0x0,MEMORY_PAGE_SIZE);
    uint32_t argc = 0;
    for(uint32_t i = 0; argv[i];i++) {argc++;}

    uint32_t* arg_ptrs = (uint32_t*)kmalloc(argc * sizeof(uint32_t));    
    uint32_t sp = KERNEL_START;
    if (arg_ptrs){
        uint32_t write_ptr = MEMORY_PAGE_SIZE;
        
        // argv data
        for (int i = argc - 1; i >= 0; i--){
            uint32_t len = strlen(argv[i]) + 1;
            sp -= len;
            write_ptr -= len;
            memcpy(&page[write_ptr],argv[i],len);
            arg_ptrs[i] = sp;

        }
        // align the pointers
        sp &= ~0x0f;
        write_ptr &= ~0x0f;


        sp -= sizeof(char*);
        write_ptr -= sizeof(char*);
        *(uint32_t*)&page[write_ptr] = 0;

        // argv pointers
        for (int i = argc - 1; i >= 0; i--){
            sp -= sizeof(char*);
            write_ptr -= sizeof(char*);
            *(uint32_t*)&page[write_ptr] = arg_ptrs[i];
        }

        sp -= sizeof(char*);
        write_ptr -= sizeof(char*);
        *(uint32_t*)&page[write_ptr] = sp + sizeof(char*);

        sp -= sizeof(char*);
        write_ptr -= sizeof(char*);
        *(uint32_t*)&page[write_ptr] = argc;

        sp -= sizeof(char*); // go one lower since [esp] is expected to be the return address
        
        kfree(arg_ptrs);
    }else{
        sp -= 0x5; // to 0xbffffffb
    }
    state->regs.esp = sp;
    state->regs.ebp = sp;


    mem_unmap_page(TEMP_KERNEL_COPY_ADDR);
    mem_map_page_in_dir(proc->page_dir,KERNEL_START - MEMORY_PAGE_SIZE,page_phys, PAGE_FLAG_USER | PAGE_FLAG_WRITE);
}

uint32_t create_user_process(unsigned char* binary, uint32_t size,unsigned char* process_name,uint8_t priv_lvl, unsigned char* argv[],process_fds_init_t* start_fds) {
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
    
    int pid = get_pid();
    if (pid == -1) return 0;
    process->process_id = pid;
    
    memset(process->fd_table,0,MAX_FDS);

    generic_file_t* stdin;
    generic_file_t* stdout;
    generic_file_t* stderr;
    log("SPAWNING PROC");
    log(process_name);

    uint32_t name_len = strlen(process_name);
    process->process_name = (unsigned char*)kmalloc(name_len + 1);
    memcpy(process->process_name,process_name,name_len + 1);

    process->priv_lvl = priv_lvl;

    overwrite_current_proc(process);
    if (!start_fds){
        // it is important that these are individua files to avoid double frees when exiting
        stdin = fs_open("dev/null",FILE_FLAG_NONE);
        stdout = fs_open("dev/null",FILE_FLAG_NONE);
        stderr = fs_open("dev/null",FILE_FLAG_NONE);  
    }else{
        //stderr should be 0 for now
        stdin = fs_open(start_fds->stdin_filename,FILE_FLAG_READ);
        if (!stdin) stdin = fs_open("dev/null",FILE_FLAG_NONE);

        stdout = fs_open(start_fds->stdout_filename,FILE_FLAG_WRITE);
        if (!stdout) stdout = fs_open("dev/null",FILE_FLAG_NONE);

        stderr = fs_open(start_fds->stderr_filename,FILE_FLAG_WRITE);
        if (!stderr) stderr = fs_open("dev/null",FILE_FLAG_NONE);
    }
    restore_active_proc();
    process->fd_table[FD_STDIN] = stdin;
    process->fd_table[FD_STDOUT] = stdout;
    process->fd_table[FD_STDERR] = stderr;
    
    process->vm_areas = nullptr;
    process->running = 0;
    process->kernel_stack = kernel_stack;
    

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

    setup_arguments(process,argv);

    // first page is managed by the argv setup
    for (uint32_t i = 1; i < USER_STACK_PAGES_PER_PROCESS;i++){
        uint32_t stack_mem = pmm_alloc_page_frame();
        mem_map_page_in_dir(process->page_dir,USER_STACK_VMEMORY_START - ((i + 1) * MEMORY_PAGE_SIZE),stack_mem,PAGE_FLAG_WRITE | PAGE_FLAG_USER);
        
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
    //esp and ebp are already set up
    state->regs.ss = user_mode_data_segment_selector;
    state->exec_state = EXEC_STATE_RUNNING;
    set_interrupt_status(int_save);
}

void dispatch_user_process(uint32_t pid){
    user_process_t* process = get_user_process_by_pid(pid);
    if (process->running) return;
    process->running = 1;
    uint32_t* old_pd = mem_get_current_page_dir();
    mem_change_page_dir(process->page_dir); 
    set_kernel_stack(process->kernel_stack + MEMORY_PAGE_SIZE);
    load_registers();
    mem_change_page_dir(old_pd);
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

    for (uint32_t i = 0; i < MAX_FDS;i++){
        if(!process->fd_table[i]) break;

        sys_close(process,i);
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

int run(char* filepath,unsigned char* argv[],process_fds_init_t* start_fds,uint8_t priv_lvl){
    inode_t* file = get_inode_by_path(filepath);

    if (!file){
        error("Fetching file failed");
        return -1;
    }

    int fd = sys_open(&global_kernel_process,filepath,FILE_FLAG_READ | FILE_FLAG_EXEC);
    
    if (fd < 0){
        error("FD failed");
        return -1;
    }
    
    unsigned char* binary = (unsigned char*)kmalloc(file->size);
    
    int ret_val = sys_read(&global_kernel_process,fd,binary,file->size);
    
    sys_close(&global_kernel_process,fd);
    if (ret_val < 0){
        kfree(binary);
        error("Failed to read executable file");
        return -1;
    }


    uint8_t old_int_status = get_interrupt_status();
    disable_interrupts();

    if (!argv) {
        unsigned char* argv2[] = {0};
        argv = argv2;
    }

    uint32_t pid = create_user_process(binary,file->size,filepath,priv_lvl,argv,start_fds);

    if (!pid) {
        kfree(binary);
        error("Creating user process failed");
        return -1;
    }

    dispatch_user_process(pid);
    set_interrupt_status(old_int_status);

    return 0;

}