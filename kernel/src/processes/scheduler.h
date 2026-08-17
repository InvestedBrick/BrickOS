
#ifndef INCLUDE_SCHEDULER_H
#define INCLUDE_SCHEDULER_H

#include "../tables/interrupts.h"
#include "../drivers/timer/pit.h"
#include "process.h"
#include "../filesystem/filesystem.h"
#include <stdbool.h>

#define KERNEL_THREAD_CREAT_SUCCESS 0x0
#define KERNEL_THREAD_CREAT_FAILURE 0x1

typedef enum {
    WAITING,
    TIMED_OUT,
    SIGNALED
}wait_state_e;

struct thread;
typedef struct thread thread_t;

typedef enum {
    EXEC_STATE_INIT,
    EXEC_STATE_RUNNING,
    EXEC_STATE_SLEEPING,
    EXEC_STATE_DEAD,
    EXEC_STATE_FINALIZED,
    EXEC_STATE_DONT_SCHEDULE
}exec_state_e;

typedef struct sleeping_thread {
    struct sleeping_thread* next;
    uint64_t wakeup_tick;
    thread_t* thread;
}sleeping_thread_t;

typedef struct lock_waiting_thread{
    struct lock_waiting_thread* next;
    wait_state_e wait_state;
    thread_t* thread;
}lock_waiting_thread_t;

typedef struct thread{
    uint32_t tid;
    exec_state_e exec_state;

    interrupt_stack_frame_t regs;

    struct process* owner_proc; 
    uint64_t kernel_rsp; 
    uint64_t init_rsp;
    uint64_t init_user_ss;
    inode_t* active_dir;

   sleeping_thread_t sleeping_thread;

    lock_waiting_thread_t lock_wthread;

    struct thread* next;
    struct thread* next_proc_thread;
} thread_t;

#define TASK_SWITCH_DELAY_MS 10
#define MS_TO_TICKS(ms) (((ms) * DESIRED_STANDARD_FREQ) / 1000)
#define TICKS_TO_MS(ticks) (((ticks) * 1000) / DESIRED_STANDARD_FREQ)
#define TASK_SWITCH_TICKS MS_TO_TICKS(TASK_SWITCH_DELAY_MS)

#define THREAD_ETERNAL_SLEEP (uint64_t)-1

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
int add_thread(struct process* usr_proc);

/**
 * remove_thread:
 * Removes a thread from all its associated linked lists and frees it
 * @param thread The thread
 */
void remove_thread(thread_t* thread);

/**
 * get_thread_by_tid:
 * Returns a thread with a given tid
 * @param tid The thread id of the thread
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
 * @param th The thread to send to sleep
 * @param sleep_ticks The number of ticks to make the thread sleep
 */
void add_sleeping_thread(thread_t* thread,uint64_t sleep_ticks);

/**
 * wakeup_thread:
 * Manually awakes a sleeping thread
 * @param thread The sleeping thread to awake
 */
void wakeup_thread(thread_t* thread);

/**
 * yield:
 * Runs the yield interrupt to switch execution to another thread
 */
void yield();

/**
 * create_kernel_worker_thread.
 * creates a kernel worker thread that will starts its executation as the specified function
 * @param entry_func A function pointer to the entry function
 * @return The created kernel thread if sucess, otherwise nullptr
 */
thread_t* create_kernel_worker_thread(void (*entry_func)());
#endif