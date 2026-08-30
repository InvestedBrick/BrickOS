#include "process.h"
#include "../memory/memory.h"
#include "../memory/kmalloc.h"
#include "../utilities/vector.h"
#include <shared/util.h>
#include "../io/log.h"
#include "scheduler.h"
#include "../tables/tss.h"
#include "../tables/interrupts.h"
#include "../tables/syscalls.h"
#include "../filesystem/vfs/vfs.h"
#include "../drivers/PS2/keyboard/keyboard.h"
#include "../screen/screen.h"
#include "../filesystem/filesystem.h"
#include "../filesystem/file_operations.h"
#include "../kernel_header.h"
#include <shared/syscall_defines.h>
#include "../filesystem/devices/devs.h"
#include "scheduler.h"
#include "../tables/timer_callbacks.h"
#include "../utilities/elf_parser.h"
#include "../tables/gdt.h"
#include "../processes/spinlocks.h"

vector_t process_vector;
spinlock_t proc_vec_lock;

static uint8_t pid_used[MAX_PIDS] = {0};
static uint32_t next_pid = 1;
process_t* overwrite_proc = 0;
struct process global_kernel_process;


void finish_up_root_process(){
    global_kernel_process.fd_table[FD_STDIN] = fs_open("dev/kb0",FILE_FLAG_NONE);
    global_kernel_process.fd_table[FD_STDOUT] = fs_open("dev/null",FILE_FLAG_NONE);

}

void create_root_process(){
    init_processes();

    memset(global_kernel_process.fd_table,0,MAX_FDS * sizeof(generic_file_t*));

    global_kernel_process.pml4 = mem_get_current_pml4_table();
    global_kernel_process.process_id = get_pid();
    global_kernel_process.vm_areas = 0;
    global_kernel_process.priv_lvl = PRIV_ALUCARD; // the all-powerful
    global_kernel_process.process_name = kmalloc(sizeof("root"));
    global_kernel_process.page_alloc_start = 0x0;

    memcpy(global_kernel_process.process_name,"root",sizeof("root"));
    global_kernel_process.running = 1;
    
    global_kernel_process.main_thread = (thread_t*)kmalloc(sizeof(thread_t));
    memset(global_kernel_process.main_thread,0x0,sizeof(thread_t));
    global_kernel_process.main_thread->tid = get_pid();
    //active dir is set in init_filesystem
    global_kernel_process.main_thread->owner_proc = &global_kernel_process;
    global_kernel_process.main_thread->exec_state = EXEC_STATE_DONT_SCHEDULE;

    vector_append(&process_vector,(vector_data_t)&global_kernel_process);

}


process_t* get_process_by_pid(uint32_t pid){
    spinlock_acquire(&proc_vec_lock);
    for (uint32_t i = 0; i < process_vector.size;i++){
        if (((process_t*)(process_vector.data[i]))->process_id == pid){
            spinlock_release(&proc_vec_lock);
            return (process_t*)(process_vector.data[i]);
        }
    }
    spinlock_release(&proc_vec_lock);
    return 0;
}

process_t* get_current_process(){

    if (overwrite_proc) return overwrite_proc;

    thread_t* curr_thread = get_current_thread();

    return curr_thread->owner_proc;

}

// this is the stuff that is created when you just want stuff to work
void overwrite_current_proc(process_t* proc){
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

int assign_fd(process_t* proc,generic_file_t* file){
    for (int i = 3; i < MAX_FDS; ++i) {
        if (proc->fd_table[i] == 0) {
            proc->fd_table[i] = file;
            return i;
        }
    }
    return -1; 
}

void free_fd(process_t* proc, generic_file_t* file){
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

Elf64_Phdr* find_responsible_phdr(process_t* p, uint64_t virt_addr){
    for (uint32_t i = 0; i < p->n_phdrs;i++){
        if (p->phdrs[i].p_vaddr <= virt_addr && virt_addr < p->phdrs[i].p_vaddr + p->phdrs[i].p_memsz){
            return &p->phdrs[i];
        }
    }
    return nullptr;
}
Elf64_Phdr* find_lowest_responsible_phdr(process_t* p, uint64_t low){
    uint64_t high = ALIGN_UP(low + 1,MEMORY_PAGE_SIZE); // low + 1 to handle when low is exactly the start of a page
    uint32_t min = high;
    if (low >= high) error("phdr alignment broke somehow");
    Elf64_Phdr* lowest = nullptr;
    for (uint32_t i = 0; i < p->n_phdrs;i++){
        if (p->phdrs[i].p_vaddr <= high && p->phdrs[i].p_vaddr + p->phdrs[i].p_memsz >= low){
            // intersecting
            if (p->phdrs[i].p_vaddr < min){
                min = p->phdrs[i].p_vaddr;
                lowest = &p->phdrs[i];
            }
        }
    }
    return lowest;
}


bool handle_phdr_mapping(process_t* p, uint64_t fault_addr){
    uint64_t aligned_fault_addr = ALIGN_DOWN(fault_addr,MEMORY_PAGE_SIZE);
    uint64_t page_top = aligned_fault_addr + MEMORY_PAGE_SIZE;

    // a bit unconventional but since this is only internal it's fine
    generic_file_t* file = fs_open_inode(p->file_inode,FILE_FLAG_READ,"");
    int fd = assign_fd(p,file);
    
    if (fd < 0) return false;

    bool success = false;
    uint64_t low = aligned_fault_addr;
    uint64_t page = pmm_alloc_page_frame();
    mem_map_page(USER_SCRATCH_PAGE,page,PAGE_FLAG_WRITE | PAGE_FLAG_USER);
    uint32_t flags = PAGE_FLAG_USER;
    while(true){
        if (low > page_top) break;
        Elf64_Phdr* phdr = find_lowest_responsible_phdr(p, low);
        if (!phdr || phdr->p_type != PT_LOAD) break;
        // Handle the phdr mapping
        uint64_t top = min(phdr->p_vaddr + phdr->p_memsz,page_top);
        uint64_t bottom = max(phdr->p_vaddr,low);
        uint64_t file_off = phdr->p_offset + (bottom - phdr->p_vaddr);
        if ((top - bottom) == 0) break;
        
        uint32_t page_off = bottom - aligned_fault_addr;

        
        if (phdr->p_filesz > 0){
            sys_seek(p,fd,file_off,SEEK_SET);
        }

        uint64_t read_top = min(phdr->p_vaddr + phdr->p_filesz,top);
        uint64_t read_size = max(read_top - bottom,0);
        if (read_size > 0){
            sys_read(p,fd,(unsigned char*)(USER_SCRATCH_PAGE + page_off),read_size);
        }
        uint64_t zero_size = top - max(read_top,bottom);
        if (zero_size > 0){
            memset((void*)(USER_SCRATCH_PAGE + page_off + read_size),0,zero_size);
        }
        if (phdr->p_flags & PF_W) flags |= PAGE_FLAG_WRITE;
        low = top + 1;
        success = true;
        
    }
    mem_unmap_page(USER_SCRATCH_PAGE);
    if (success){
        mem_map_page(aligned_fault_addr,page,flags);
    }   
    sys_close(p,fd);
    if (!success){
        pmm_free_page_frame(page);
    }
    return success;

}


void setup_arguments(process_t* proc,unsigned char* argv[]){
    uint64_t page_phys = pmm_alloc_page_frame();
    thread_t* main_thread = proc->main_thread;
    if (!main_thread) error("Failed to get main thread");
    // map temporarily to write memory into it
    mem_map_page(USER_SCRATCH_PAGE,page_phys,PAGE_FLAG_WRITE | PAGE_FLAG_USER);
    unsigned char* page = (unsigned char*)USER_SCRATCH_PAGE;
    memset(page,0x0,MEMORY_PAGE_SIZE);
    uint64_t argc = 0;
    if (argv){
        for(uint32_t i = 0; argv[i];i++) {argc++;}
    }
    
    uint64_t* arg_ptrs = (uint64_t*)kmalloc(argc * sizeof(uint64_t));    
    uint64_t sp = USER_STACK_VMEMORY_START;
    uint32_t write_ptr = MEMORY_PAGE_SIZE;
        
    // argv data
    for (int64_t i = argc - 1; i >= 0; i--){
        uint32_t len = strlen(argv[i]) + 1;
        if (len > write_ptr) error("Too many arguments to fit on stack");
        sp -= len;
        write_ptr -= len;
        memcpy(&page[write_ptr],argv[i],len);
        arg_ptrs[i] = sp;
    }
    // align stack to 16 bits
    sp &= ~0xfull;
    write_ptr &= ~0xfull;

    // Null terminator for argv
    sp -= sizeof(char*);
    write_ptr -= sizeof(char*);
    *(uint64_t*)&page[write_ptr] = 0;

    // argv pointers
    for (int64_t i = argc - 1; i >= 0; i--){
        sp -= sizeof(char*);
        write_ptr -= sizeof(void*);
        *(uint64_t*)&page[write_ptr] = arg_ptrs[i];
    }

    uint64_t argv_ptr = sp;

    sp -= sizeof(void*);
    write_ptr -= sizeof(void*);
    *(uint64_t*)&page[write_ptr] = sp + sizeof(void*);

    // not needed rn but for future ELF compatibility
    sp -= sizeof(void*);
    write_ptr -= sizeof(void*);
    *(uint64_t*)&page[write_ptr] = argc;

    sp &= ~0xfull;
    sp -= sizeof(void*); // proper stack align
    
    main_thread->init_rsp = sp;
    main_thread->regs.rdi = argc;
    main_thread->regs.rsi = argv_ptr;
    
    mem_unmap_page(USER_SCRATCH_PAGE);

    uint64_t stack_base = USER_STACK_VMEMORY_START & ~(MEMORY_PAGE_SIZE - 1);
    mem_map_page_in_pml4(proc->pml4, stack_base, page_phys, PAGE_FLAG_USER | PAGE_FLAG_WRITE);

    kfree(arg_ptrs); // might be null, but kfree does check that
}

uint32_t create_process(unsigned char* file_path,uint8_t priv_lvl, unsigned char* argv[],process_fds_init_t* start_fds) {
    uint32_t f = irq_save();

    Elf64_Ehdr ehdr;
    if (!validate_elf(file_path,&ehdr)){
        errorf("Tried to spawn invalid ELF: %s",file_path);
        irq_restore(f);
        return 0;
    }

    int pid = get_pid();
    if (pid == -1){
        irq_restore(f);
        return 0;
    }

    process_t* process = (process_t*)kmalloc(sizeof(process_t));
    
    process->process_id = pid;
    
    memset(process->fd_table,0,MAX_FDS * sizeof(generic_file_t*));

    generic_file_t* stdin;
    generic_file_t* stdout;
    generic_file_t* stderr;
    logf("Spawning process: %s",file_path);

    uint32_t name_len = strlen(file_path);
    process->process_name = (unsigned char*)kmalloc(name_len + 1);
    memcpy(process->process_name,file_path,name_len + 1);

    process->priv_lvl = priv_lvl;

    overwrite_current_proc(process);
    if (!start_fds){
        // it is important that these are individual files to avoid double frees when exiting
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
    
    inode_t* file = get_inode_by_path(file_path);
    file->perms &= ~FS_FILE_PERM_WRITABLE;
    process->file_inode = file;

    uint32_t code_data_pages = CEIL_DIV(file->size,MEMORY_PAGE_SIZE) + 1; // add one safety page
    
    process->page_alloc_start = code_data_pages * MEMORY_PAGE_SIZE;
    
    spinlock_acquire(&proc_vec_lock);
    vector_append(&process_vector,(vector_data_t)process); // too lazy to implement a vector for structs
    spinlock_release(&proc_vec_lock);

    uint64_t* pml4 = create_user_pml4_table();
    
    process->pml4 = pml4;
    process->main_thread = nullptr;
    int tid = add_thread(process);
    if (tid == -1) {
        irq_restore(f);
        return 0;
    }
    process->main_thread = get_thread_by_tid(tid);

    process->n_phdrs = ehdr.e_phnum;
    process->phdrs = extract_elf_phdrs(file_path);
    process->main_thread->regs.rip = ehdr.e_entry;

    setup_arguments(process,argv);
        
    // first page is managed by the argv setup
    uint64_t stack_base = USER_STACK_VMEMORY_START & ~(MEMORY_PAGE_SIZE - 1);
    for (uint32_t i = 1; i < USER_STACK_PAGES_PER_PROCESS; i++){
        uint64_t stack_mem = pmm_alloc_page_frame();
        mem_map_page_in_pml4(process->pml4, stack_base - (i * MEMORY_PAGE_SIZE), stack_mem, PAGE_FLAG_WRITE | PAGE_FLAG_USER);
    }

    irq_restore(f);

    return process->process_id;

}

void load_registers(thread_t* main_thread){
    uint32_t f = irq_save();
    // we do a little pretending here so that when the scheduler returns with these values, everything starts
    
    main_thread->regs.fs = USER_DS;
    main_thread->regs.gs = USER_DS;

    main_thread->regs.cs = USER_CS;
    main_thread->regs.rflags = EFLAGS_IF; // enable interrupts for user
    //esp and ebp are already set up
    main_thread->init_user_ss = USER_DS;
    main_thread->exec_state = EXEC_STATE_FINALIZED;
    irq_restore(f);
}

void dispatch_process(uint32_t pid){
    process_t* process = get_process_by_pid(pid);
    if (process->running) return;
    process->running = 1;
    uint64_t* old_pml4 = mem_get_current_pml4_table();
    load_registers(process->main_thread);
}


void init_processes(){
    spinlock_init(&proc_vec_lock);
    init_vector(&process_vector);
}

int kill_process(uint32_t pid){
    if (!(pid > 0 && pid < MAX_PIDS && pid_used[pid])) return SYSCALL_FAIL;
    process_t* process;
    spinlock_acquire(&proc_vec_lock);
    for (uint32_t i = 0; i < process_vector.size;i++){
        if (((process_t*)(process_vector.data[i]))->process_id == pid){
            process = (process_t*)(process_vector.data[i]);
            vector_erase(&process_vector,i);
            break;
        }
    }
    spinlock_release(&proc_vec_lock);
    process->running = 0;

    for (uint32_t i = 0; i < MAX_FDS;i++){
        if(!process->fd_table[i]) continue;

        sys_close(process,i);
    }
    
    virt_mem_area_t* vma = process->vm_areas;
    virt_mem_area_t* prev_vma;
    while(vma){
        
        free_shrd_vma_obj(vma); // ensures shrd_obj exists
        vector_free(&vma->mapped_pages,false);
        prev_vma = vma;
        vma = vma->next;
        kfree(prev_vma);
    }
    // Threads are already dead since we should be coming from remove_thread
    free_user_pml4_table(process->pml4);
    if (process->file_inode)
        process->file_inode->perms |= FS_FILE_PERM_WRITABLE;
        
    kfree(process->phdrs);
    logf("Killed '%s'",process->process_name);
    kfree(process->process_name);
    kfree(process);
    free_pid(pid);
    return 0;
}

__attribute__((noreturn))
void enter_user_mode(struct thread* thread){

    asm volatile(
        "push %0\n\t"
        "push %1\n\t"
        "push %2\n\t"
        "push %3\n\t"
        "push %4\n\t"
        "iretq\n\t"
        :
        : "r"(thread->init_user_ss),
          "r"(thread->init_rsp),
          "r"(thread->regs.rflags),
          "r"(thread->regs.cs),
          "r"(thread->regs.rip)
        : "memory"
    );

    __builtin_unreachable();
}

__attribute__((noreturn))
void enter_kernel_thread(struct thread* thread){
    asm volatile(
        "mov %0, %%rsp\n\t"
        "jmp *%1\n\t"
        :
        : "r"(thread->init_rsp),
          "r"(thread->regs.rip)
        : "memory"
    );

    __builtin_unreachable();
}

int run(char* filepath,unsigned char* argv[],process_fds_init_t* start_fds,uint8_t priv_lvl){
    inode_t* file = get_inode_by_path(filepath);

    if (!file){
        error("Fetching file failed");
        return -1;
    }
    if (!(file->perms & FS_FILE_PERM_EXECUTABLE)) {
        error("File not executable");
        return -1;
    }
    uint32_t f = irq_save();

    uint32_t pid = create_process(filepath,priv_lvl,argv,start_fds);
    
    if (!pid) {
        error("Creating user process failed");
        irq_restore(f);
        return -1;
    }

    dispatch_process(pid);
    irq_restore(f);

    return 0;

}