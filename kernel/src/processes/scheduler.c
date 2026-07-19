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
#include "../ACPI/apic.h"
#include "../tables/timer_callbacks.h"

thread_t* t_queue;
thread_t* current_thread;
uint8_t first_switch = 1;
sleeping_thread_t* sleeping_thread_head;
extern uint32_t stack_top;

void invoke_scheduler(){
    if (!get_interrupt_status()) return;

    thread_t* curr_thread = get_current_thread();
    curr_thread->expect_sched_fault = true;
    asm volatile(
        ".global sched_fault_ip\n\t"
        ".global sched_fault_fixup\n\t"
        "sched_fault_ip:\n\t"
        "movb (%0), %%al\n\t"
        "sched_fault_fixup:\n\t"
        : : "r"((uint64_t)MAGIC_SCHED_FAULT_ADDR) : "al"
    );
}

void add_sleeping_thread(thread_t* thread, uint64_t sleep_ticks){
    sleeping_thread_t* sleepy_thread = (sleeping_thread_t*)kmalloc(sizeof(sleeping_thread_t));
    thread->exec_state = EXEC_STATE_SLEEPING;
    sleepy_thread->thread = thread;
    if (sleep_ticks == THREAD_ETERNAL_SLEEP) 
        sleepy_thread->wakeup_tick = THREAD_ETERNAL_SLEEP;
    else
        sleepy_thread->wakeup_tick = ticks + sleep_ticks;

    sleepy_thread->next = sleeping_thread_head;
    sleeping_thread_head = sleepy_thread;
}

void remove_sleeping_thread_with_prev(sleeping_thread_t* prev, sleeping_thread_t* sleepy_thread){
    if (prev == nullptr) {
        sleeping_thread_head = sleepy_thread->next;
    }else {
        prev->next = sleepy_thread->next;
    }
    kfree(sleepy_thread);
}

void remove_sleeping_thread(sleeping_thread_t* sleepy_thread){
    sleeping_thread_t* prev = nullptr;
    sleeping_thread_t* curr = sleeping_thread_head;

    while (curr != nullptr) {
        if (curr == sleepy_thread) {
            remove_sleeping_thread_with_prev(prev, curr);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

void wakeup_thread(thread_t* thread){
    if (thread->exec_state != EXEC_STATE_SLEEPING) return;

    sleeping_thread_t* curr = sleeping_thread_head;
    while(curr){
        if (curr->thread == thread) {
            remove_sleeping_thread(curr);
            thread->exec_state = EXEC_STATE_RUNNING;
            break;
        }
        curr = curr->next;
    }
}

void manage_sleeping_threads(){
    sleeping_thread_t* prev = nullptr;
    sleeping_thread_t* curr = sleeping_thread_head;

    while (curr != nullptr) {
        sleeping_thread_t* next = curr->next;

        if (curr->wakeup_tick <= ticks) {
            curr->thread->exec_state = EXEC_STATE_RUNNING;
            remove_sleeping_thread_with_prev(prev, curr);
        } else {
            prev = curr;
        }

        curr = next;
    }
}

thread_t* get_current_thread(){
    return current_thread;
}

void init_scheduler(){
    t_queue = global_kernel_process.main_thread;
    current_thread = t_queue;

    // timer callbacks
    kworker_timer_callbacks = create_kernel_worker_thread(kthread_worker_timer_callbacks);

    init_vector(&timer_callbacks_vec);
}

thread_t* get_thread_by_tid(uint32_t tid){
    thread_t* node = t_queue;
    while (node) {
        if (node->tid == tid) return node;
        node = node->next;
    }
    return 0;
}

thread_t* create_thread(user_process_t* owner_proc){
    if (!t_queue) {
        error("Process queue not initialized");
        return nullptr;
    }
    thread_t* thread = (thread_t*)kmalloc(sizeof(thread_t));
    memset(thread,0x0,sizeof(thread_t));
    thread->expect_sched_fault = false;
    thread->next = 0;
    
    int tid = get_pid(); // need to put it here so that compiler does not generate weird opcode for some reason
    if (tid == -1) {error("Failed to assign thread id"); return nullptr;}
    
    thread->next_proc_thread = 0;
    thread->tid = tid;
    thread->owner_proc = owner_proc;
    thread->exec_state = EXEC_STATE_INIT;
    thread->active_dir = current_thread->active_dir;

    return thread;
}

void enqueue_thread(thread_t* thread){
    // add to main thread queue
    thread_t* last = t_queue;

    user_process_t* owner_proc = thread->owner_proc;

    while(last->next) {last = last->next;}
    last->next = thread;

    // add to user process thread queue
    if (!owner_proc->main_thread) {owner_proc->main_thread = thread;}
    else{
        last = owner_proc->main_thread;
        while(last->next_proc_thread) {last = last->next_proc_thread;}
        last->next_proc_thread = thread; 
    }
}

int add_thread(struct user_process* usr_proc){

    thread_t* thread = create_thread(usr_proc);
    if (!thread) return -1;

    enqueue_thread(thread);

    return thread->tid;

}

thread_t* create_kernel_worker_thread(void (*entry_func)()){
    thread_t* kthread  = create_thread(&global_kernel_process);
    if (!kthread) return nullptr;

    memset(&kthread->regs,0x0,sizeof(kthread->regs));

    kthread->regs.rip = (uint64_t)entry_func;
    uint64_t phys = pmm_alloc_page_frame();
    kthread->init_rsp = map_somewhere_rw(phys) + MEMORY_PAGE_SIZE; // one page, use it wisely

    enqueue_thread(kthread);

    kthread->exec_state = EXEC_STATE_FINALIZED;

    return kthread;
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

        if (!t_queue->next->next) {
            current_thread = global_kernel_process.main_thread;
            shutdown();
        }
    }
    while((candidate->exec_state != EXEC_STATE_RUNNING && candidate->exec_state != EXEC_STATE_FINALIZED) || (candidate == t_queue));

    return candidate;
}

void switch_task(interrupt_stack_frame_t* regs){
    // only switch when the scheduler was set up 
    if (!t_queue->next){
        return;
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
    // send EOI since we cant return to interrupt_handler
    write_apic_register(APIC_REG_EOI,0x0); 

    if (current_thread->exec_state == EXEC_STATE_FINALIZED){
        current_thread->exec_state = EXEC_STATE_RUNNING;
        if (current_thread->owner_proc->process_id == global_kernel_process.process_id){
            enter_kernel_thread(current_thread); // does not return
        }else{
            enter_user_mode(current_thread); // does not return
        }
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

    if (thread->owner_proc->process_id == global_kernel_process.process_id){
        mem_unmap_page(thread->init_rsp - MEMORY_PAGE_SIZE);
    }

    kfree(thread);
}