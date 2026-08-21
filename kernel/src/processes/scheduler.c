#include "scheduler.h"
#include "../memory/kmalloc.h"
#include <shared/util.h>
#include "../memory/memory.h"
#include "../io/log.h"
#include "process.h"
#include "../tables/tss.h"
#include "../kernel_header.h"
#include "../tables/syscalls.h"
#include "../filesystem/filesystem.h"
#include "../kernel_header.h"
#include "../utilities/vector.h"
#include "../ACPI/apic.h"
#include "../tables/timer_callbacks.h"
#include "spinlocks.h"
#include "../tables/gdt.h"
#include "kworker.h"
#include <stdbool.h>

spinlock_t t_queue_lock;
thread_t* t_queue;

spinlock_t sleeping_thread_queue_lock;
sleeping_thread_t* sleeping_thread_head;
thread_t* current_thread;

uint8_t first_switch = 1;
extern uint32_t stack_top;

void yield(){
    if (!interrupts_enabled()) return;

    asm volatile ("int $" STR(INT_YIELD));
}

static void insert_sleeping_thread(sleeping_thread_t* thread){
    // insert sorted by wakeup tick
    spinlock_acquire(&sleeping_thread_queue_lock);
    sleeping_thread_t* curr = sleeping_thread_head;
    if (!curr || thread->wakeup_tick <= curr->wakeup_tick) {

        thread->next = sleeping_thread_head;
        sleeping_thread_head = thread;
    }else{
        while (curr->next && curr->next->wakeup_tick < thread->wakeup_tick) curr = curr->next;
        thread->next = curr->next;
        curr->next = thread;
    }
    spinlock_release(&sleeping_thread_queue_lock);
}

void add_sleeping_thread(thread_t* thread, uint64_t sleep_ticks){
    if (thread->exec_state == EXEC_STATE_SLEEPING) return;
    thread->exec_state = EXEC_STATE_SLEEPING;
    thread->sleeping_thread.thread = thread;
    thread->sleeping_thread.wakeup_tick = ticks + min(UINT64_MAX - ticks, sleep_ticks);
    insert_sleeping_thread(&thread->sleeping_thread);

}

void wakeup_thread(thread_t* thread){
    if (thread->exec_state != EXEC_STATE_SLEEPING) return;
    spinlock_acquire(&sleeping_thread_queue_lock);
    sleeping_thread_t* prev = nullptr;
    sleeping_thread_t* curr = sleeping_thread_head;
    while(curr){
        if (curr->thread == thread) {

            if (prev)
                prev->next = curr->next;
            else
                sleeping_thread_head = curr->next;
            
            thread->sleeping_thread.next = nullptr;
            thread->exec_state = EXEC_STATE_RUNNING;
            spinlock_release(&sleeping_thread_queue_lock);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
    spinlock_release(&sleeping_thread_queue_lock);
}

void manage_sleeping_threads(){
    spinlock_acquire(&sleeping_thread_queue_lock);
    sleeping_thread_t* curr = sleeping_thread_head;
    while(curr->wakeup_tick <= ticks){
        curr->thread->lock_wthread.wait_state = TIMED_OUT;
        curr->thread->exec_state = EXEC_STATE_RUNNING;
        curr = curr->next;
    }
    sleeping_thread_head = curr;
    spinlock_release(&sleeping_thread_queue_lock);
}

thread_t* get_current_thread(){
    return current_thread;
}

void init_scheduler(){
    spinlock_init(&t_queue_lock);
    spinlock_init(&sleeping_thread_queue_lock);
    t_queue = global_kernel_process.main_thread;
    current_thread = t_queue;

    for (uint32_t i = 0; i < N_KWORKERS; i++){
        add_kworker();
    }
}

thread_t* get_thread_by_tid(uint32_t tid){
    spinlock_acquire(&t_queue_lock);
    thread_t* node = t_queue;
    while (node) {
        if (node->tid == tid) {
            spinlock_release(&t_queue_lock);
            return node;
        }
        node = node->next;
    }
    spinlock_release(&t_queue_lock);
    return 0;
}

static thread_t* create_thread(process_t* owner_proc){
    
    thread_t* thread = (thread_t*)kmalloc(sizeof(thread_t));
    memset(thread,0x0,sizeof(thread_t));
    thread->next = nullptr;
    
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
    spinlock_acquire(&t_queue_lock);
    if (!t_queue) {
        error("Process queue not initialized");
        spinlock_release(&t_queue_lock);
        return;
    }
    thread_t* last = t_queue;

    process_t* owner_proc = thread->owner_proc;

    while(last->next) {last = last->next;}
    last->next = thread;

    // add to user process thread queue
    if (!owner_proc->main_thread) {owner_proc->main_thread = thread;}
    else{
        last = owner_proc->main_thread;
        while(last->next_proc_thread) {last = last->next_proc_thread;}
        last->next_proc_thread = thread; 
    }
    spinlock_release(&t_queue_lock);
}

int add_thread(struct process* usr_proc){

    thread_t* thread = create_thread(usr_proc);
    if (!thread) return -1;

    enqueue_thread(thread);

    return thread->tid;

}

thread_t* create_kernel_worker_thread(void (*entry_func)()){
    thread_t* kthread  = create_thread(&global_kernel_process);
    if (!kthread) return nullptr;
    memset(kthread,0x0,sizeof(thread_t));

    kthread->regs.rip = (uint64_t)entry_func;
    kthread->regs.rflags = EFLAGS_IF;
    kthread->regs.cs = KERNEL_CS;
    kthread->active_dir = get_inode_by_id(FS_ROOT_DIR_ID);
    uint64_t phys = pmm_alloc_page_frame();
    kthread->init_rsp = map_somewhere_rw(phys) + MEMORY_PAGE_SIZE; // one page, use it wisely
    kthread->owner_proc = &global_kernel_process;
    enqueue_thread(kthread);

    kthread->exec_state = EXEC_STATE_FINALIZED;

    return kthread;
}

thread_t* find_schedule_candidate(){
    thread_t* candidate = current_thread;
    do {
        
        
        if (candidate->next) candidate = candidate->next;
        else candidate = t_queue;

        if (!t_queue->next->next) {
            //TODO: install actually correct shutdown procedure
            current_thread = global_kernel_process.main_thread;
            shutdown();
        }
    }
    while((candidate->exec_state != EXEC_STATE_RUNNING && candidate->exec_state != EXEC_STATE_FINALIZED) || (candidate == t_queue));

    return candidate;
}

void switch_task(interrupt_stack_frame_t* regs){
    
    uint32_t f = spinlock_acquire_irq(&t_queue_lock);
    
    // only switch when the scheduler was set up 
    if (!t_queue || !t_queue->next){
        spinlock_release_irq(&t_queue_lock,f);
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

    spinlock_release_irq(&t_queue_lock,f);
    
    // send EOI since we cant return to interrupt_handler
    if (regs->interrupt_number != INT_YIELD)
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
    spinlock_acquire(&t_queue_lock);

    thread_t* before_thread = t_queue;
    while(before_thread->next && before_thread->next != thread) before_thread = before_thread->next;
        
    before_thread->next = thread->next;

    before_thread = thread->owner_proc->main_thread;
    if (before_thread == thread){
        // thread is main thread
        thread->owner_proc->main_thread = thread->next_proc_thread;
        if (!thread->next_proc_thread) {// this was the last thread
            if (kill_process(thread->owner_proc->process_id) < 0) warnf("Failed to kill user process '%s'", thread->owner_proc->process_name);      
        }

        kfree(thread);
        spinlock_release(&t_queue_lock);
        return;
    }

    while(before_thread->next_proc_thread && before_thread->next_proc_thread != thread) before_thread = before_thread->next_proc_thread;
    before_thread->next_proc_thread = thread->next_proc_thread;

    if (thread->owner_proc->process_id == global_kernel_process.process_id){
        mem_unmap_page(thread->init_rsp - MEMORY_PAGE_SIZE);
    }

    kfree(thread);
    spinlock_release(&t_queue_lock);
}