#include "scheduler.h"
#include "../memory/kmalloc.h"
#include "../util.h"
#include "../memory/memory.h"
#include "../io/log.h"
#include "user_process.h"
#include "../tables/tss.h"
#include "../kernel_header.h"
#include "../tables/syscalls.h"
#include "../filesystem/filesystem.h"
#include "../kernel_header.h"
#include "../vector.h"
process_state_t* p_queue;
process_state_t* current_proc;
uint8_t locked = 0;
uint8_t first_switch = 1;
vector_t sleeping_procs;
extern uint32_t stack_top;

void add_sleeping_process(process_state_t* proc,uint32_t sleep_ticks){
    sleeping_proc_t* sleepy_proc = (sleeping_proc_t*)kmalloc(sizeof(sleeping_proc_t));
    proc->exec_state = EXEC_STATE_SLEEPING;
    sleepy_proc->proc = proc;
    sleepy_proc->wakeup_tick = ticks + sleep_ticks;

    vector_append(&sleeping_procs,(uint32_t)sleepy_proc);
}

void remove_sleeping_process(sleeping_proc_t* sleepy_proc){
    vector_erase_item(&sleeping_procs,(uint32_t)sleepy_proc);
    kfree(sleepy_proc);
}

void manage_sleeping_processes(){
    sleeping_proc_t* waked_up_procs[256];
    memset(waked_up_procs,0,256);
    uint32_t waked_up_procs_idx = 0;

    for (uint32_t i = 0; i < sleeping_procs.size;i++){
        sleeping_proc_t* sleepy_proc = (sleeping_proc_t*)sleeping_procs.data[i];
        if (sleepy_proc->wakeup_tick <= ticks){
            sleepy_proc->proc->exec_state = EXEC_STATE_RUNNING;
            waked_up_procs[waked_up_procs_idx] = sleepy_proc;
            waked_up_procs_idx++; 
        }
    }

    for (uint32_t i = 0; i < waked_up_procs_idx; i++){
        remove_sleeping_process(waked_up_procs[i]);
    }
}

process_state_t* get_current_process_state(){
    return current_proc;
}

void init_scheduler(){
    init_vector(&sleeping_procs);

    p_queue = (process_state_t*)kmalloc(sizeof(process_state_t));
    p_queue->next = 0;
    current_proc = 0;

    run("modules/loop.bin",nullptr,nullptr,PRIV_STD);
    restore_kernel_memory_page_dir();

    // since the endless proc got attached to p_queue->next, we need to re-arrange this
    process_state_t* old_p_queue = p_queue;
    p_queue = p_queue->next;
    kfree(old_p_queue);
    current_proc = p_queue;
    locked = 1;
    log("Set up endless process");
}

process_state_t* get_process_state_by_page_dir(uint32_t* page_dir){
    process_state_t* node = p_queue;
    while (node) {
        if (node->pd == page_dir) return node;
        node = node->next;
    }
    return 0;
}

void add_process_state(user_process_t* usr_proc){
    if (!p_queue) {
        error("Process queue not initialized");
        return;
    }
    process_state_t* proc = (process_state_t*)kmalloc(sizeof(process_state_t));
    proc->pd = usr_proc->page_dir;
    proc->next = 0;
    proc->kernel_stack_top = usr_proc->kernel_stack + MEMORY_PAGE_SIZE;
    proc->pid = usr_proc->process_id;
    proc->exec_state = EXEC_STATE_INIT;
    process_state_t* last = p_queue;

    while(last->next) {last = last->next;}

    last->next = proc;
}

process_state_t* find_schedule_candidate(){
    process_state_t* candidate = current_proc;
    do {
        if (candidate->next) candidate = candidate->next;
        else candidate = p_queue;
    }
    while(candidate->exec_state != EXEC_STATE_RUNNING);

    return candidate;
}

void switch_task(interrupt_stack_frame_t* regs){
    // only switch when the scheduler was set up 
    if (!locked) return;
    if (!p_queue->next){
        // the loop process is the only one left
        shutdown();
    }
    uint32_t* old_pd = mem_get_current_page_dir();
    uint32_t interrupt_code = regs->interrupt_number;

    if (!first_switch){
        // first switch is from kernel -> shell, but we dont want our loop process to contain the kernels code
        memcpy(&current_proc->regs, regs,sizeof(interrupt_stack_frame_t));
    }else{
        first_switch = 0;
    }

    current_proc = find_schedule_candidate();

    set_kernel_stack(current_proc->kernel_stack_top);

    mem_change_page_dir(current_proc->pd);
    memcpy(regs,&current_proc->regs,sizeof(interrupt_stack_frame_t));
    regs->interrupt_number = interrupt_code; 
}

void remove_process_state(process_state_t* proc){
    process_state_t* before_proc = p_queue;
    while(before_proc->next && before_proc->next != proc) before_proc = before_proc->next;

    before_proc->next = proc->next;
    // page dir is freed by kill_user_process
    kfree(proc);
}