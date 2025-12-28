
#ifndef INCLUDE_SCHEDULER_H
#define INCLUDE_SCHEDULER_H

#include "../tables/interrupts.h"
#include "../drivers/timer/pit.h"
#include "user_process.h"

#define EXEC_STATE_INIT 0x1
#define EXEC_STATE_RUNNING 0x2
#define EXEC_STATE_SLEEPING 0x3
#define EXEC_STATE_DEAD 0x4

typedef struct thread{
    uint32_t tid;
    uint8_t exec_state;
    
    interrupt_stack_frame_t regs;

    struct user_process* owner_proc; 
    
    struct thread* next;
    struct thread* next_proc_thread;
} thread_t;

typedef struct {
    thread_t* thread;
    uint64_t wakeup_tick; 
}sleeping_thread_t;

#define TASK_SWITCH_DELAY_MS 10
#define TASK_SWITCH_TICKS ((TASK_SWITCH_DELAY_MS * DESIRED_STANDARD_FREQ) / 1000)


/**
 * init_scheduler:
 * Initializes the scheduler
 */
void init_scheduler();

/**
 * switch_task:
 * Switches the current task
 */
void switch_task(interrupt_stack_frame_t* regs);
/**
 * add_thread:
 * Adds a thread to the given process and its thread id
 * @param usr_proc The user process struct
 * @return The thread id
 */
int add_thread(struct user_process* usr_proc);

/**
 * remove_thread:
 * Removes a thread from all its associated linked lists and frees it
 * @param thread The thread
 */
void remove_thread(thread_t* thread);

/**
 * get_thread_by_tid:
 * Returns a thread with a given tid
 * @param tid The thread id of the process
 */
thread_t* get_thread_by_tid(uint32_t tid);

thread_t* get_current_thread();

/**
 * manage_sleeping_threads:
 * Wakes up sleeping threds if their wakeup tick has passed
 */
void manage_sleeping_threads();

/**
 * add_sleeping_thread:
 * Add a thread to the sleeping queue
 * @param th The process to send to sleep
 * @param sleep_ticks The number of ticks to make the thread sleep
 */
void add_sleeping_thread(thread_t* thread,uint32_t sleep_ticks);
#endif