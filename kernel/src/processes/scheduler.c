#include "scheduler.h"
#include "../memory/kmalloc.h"
#include "../utilities/util.h"
#include "../memory/memory.h"
#include "../io/log.h"
#include "user_process.h"
#include "../tables/tss.h"
#include "../kernel_header.h"
#include "../tables/syscalls.h"
#include "../filesystem/filesystem.h"
#include "../kernel_header.h"
#include "../utilities/vector.h"
#include <stdbool.h>

thread_t* t_queue;
thread_t* current_thread;
bool sched_init = false;
uint8_t first_switch = 1;
vector_t sleeping_threads;
extern uint32_t stack_top;

void invoke_scheduler(){
    if (!sched_init) return;
    if (!get_interrupt_status()) return;

    volatile uint8_t _drop = *(uint8_t *)(MAGIC_SCHED_FAULT_ADDR);
    (void)(_drop);
}

void add_sleeping_thread(thread_t* thread,uint32_t sleep_ticks){
    sleeping_thread_t* sleepy_thread = (sleeping_thread_t*)kmalloc(sizeof(sleeping_thread_t));
    thread->exec_state = EXEC_STATE_SLEEPING;
    sleepy_thread->thread = thread;
    sleepy_thread->wakeup_tick = ticks + sleep_ticks;

    vector_append(&sleeping_threads,(vector_data_t)sleepy_thread);
}

void remove_sleeping_thread(sleeping_thread_t* sleepy_thread){
    vector_erase_item(&sleeping_threads,(uint64_t)sleepy_thread);
    kfree(sleepy_thread);
}

void manage_sleeping_threads(){
    sleeping_thread_t* awoken_threads[256];
    memset(awoken_threads,0,sizeof(awoken_threads));
    uint32_t waked_up_procs_idx = 0;

    for (uint32_t i = 0; i < sleeping_threads.size;i++){
        sleeping_thread_t* sleepy_thread = (sleeping_thread_t*)sleeping_threads.data[i];
        if (sleepy_thread->wakeup_tick <= ticks){
            sleepy_thread->thread->exec_state = EXEC_STATE_RUNNING;
            awoken_threads[waked_up_procs_idx] = sleepy_thread;
            waked_up_procs_idx++; 
        }
    }

    for (uint32_t i = 0; i < waked_up_procs_idx; i++){
        remove_sleeping_thread(awoken_threads[i]);
    }
}

thread_t* get_current_thread(){
    return current_thread;
}

void init_scheduler(){
    init_vector(&sleeping_threads);

    t_queue = (thread_t*)kmalloc(sizeof(thread_t));
    t_queue->next = 0;
    current_thread = 0;

    run("modules/loop.bin",nullptr,nullptr,PRIV_STD);

    // since the endless proc got attached to t_queue->next, we need to re-arrange this
    thread_t* old_t_queue = t_queue;
    t_queue = t_queue->next;
    kfree(old_t_queue);
    current_thread = t_queue;
    sched_init = true;
    log("Set up endless process");
}

thread_t* get_thread_by_tid(uint32_t tid){
    thread_t* node = t_queue;
    while (node) {
        if (node->tid == tid) return node;
        node = node->next;
    }
    return 0;
}

int add_thread(struct user_process* usr_proc){
    if (!t_queue) {
        error("Process queue not initialized");
        return -1;
    }
    thread_t* thread = (thread_t*)kmalloc(sizeof(thread_t));
    memset(thread,0x0,sizeof(thread_t));
    thread->expect_sched_fault = false;
    thread->next = 0;
    int tid = get_pid(); // need to put it here so that compiler does not generate weird opcode for some reason
    thread->next_proc_thread = 0;

    if (tid == -1) {error("Failed to assign thread id"); return -1;}
    thread->tid = tid;
    thread->owner_proc = usr_proc;
    thread->exec_state = EXEC_STATE_INIT;

    // add to main thread queue
    thread_t* last = t_queue;

    while(last->next) {last = last->next;}
    last->next = thread;

    // add to user process thread queue
    if (!usr_proc->main_thread) {usr_proc->main_thread = thread;}
    else{
        last = usr_proc->main_thread;
        while(last->next_proc_thread) {last = last->next_proc_thread;}
        last->next_proc_thread = thread; 
    }

    return thread->tid;

}

thread_t* find_schedule_candidate(){
    thread_t* candidate = current_thread;
    do {
        thread_t* dead_thread = nullptr;
        
        // cannot kill current thread since then we might no longer have a valid address space
        if (candidate->exec_state == EXEC_STATE_DEAD && candidate != current_thread) dead_thread = candidate; 
        if (candidate->next) candidate = candidate->next;
        else candidate = t_queue;
        if (dead_thread) remove_thread(dead_thread);
    }
    while(candidate->exec_state != EXEC_STATE_RUNNING && candidate->exec_state != EXEC_STATE_FINALIZED);

    return candidate;
}

void switch_task(interrupt_stack_frame_t* regs){
    // only switch when the scheduler was set up 
    if (!sched_init) return;
    if (!t_queue->next){
        // the loop process is the only one left
        shutdown();
    }

    thread_t* old_thread = current_thread;
    old_thread->kernel_rsp = (uint64_t)regs;

    if (!first_switch){
        memcpy(&current_thread->regs, regs,sizeof(interrupt_stack_frame_t));
    } else{first_switch = 0;} // dont copy kernel rip etc

    current_thread = find_schedule_candidate();

    if (old_thread->owner_proc->process_id != current_thread->owner_proc->process_id){
        set_kernel_stack(current_thread->owner_proc->kernel_stack_top);
        mem_set_current_pml4_table(current_thread->owner_proc->pml4);
    }

    if (current_thread->exec_state == EXEC_STATE_FINALIZED){
        current_thread->exec_state = EXEC_STATE_RUNNING;
        enter_user_mode(current_thread); // does not return
    }
    
    asm volatile( "mov %0, %%rsp\n\t"
                  "jmp return_from_interrupt\n\t"
                  : : "r"(current_thread->kernel_rsp) 
                  : "memory"
                );
    
    __builtin_unreachable();
}

void remove_thread(thread_t* thread){
    if (!thread) return;
    thread_t* before_thread = t_queue;
    while(before_thread->next && before_thread->next != thread) before_thread = before_thread->next;
        
    before_thread->next = thread->next;

    before_thread = thread->owner_proc->main_thread;
    if (before_thread == thread){
        // thread is main thread
        thread->owner_proc->main_thread = thread->next_proc_thread;
        if (thread->next_proc_thread == 0) {// this was the last thread
            if (kill_user_process(thread->owner_proc->process_id) < 0) warnf("Failed to kill user process '%s'", thread->owner_proc->process_name);           
        }
        kfree(thread);
        return;
    }

    while(before_thread->next_proc_thread && before_thread->next_proc_thread != thread) before_thread = before_thread->next_proc_thread;
    before_thread->next_proc_thread = thread->next_proc_thread;

    kfree(thread);
}