#ifndef INCLUDE_SPINLOCKS_H
#define INCLUDE_SPINLOCKS_H

#include <stdatomic.h>
#include <stdint.h>
#include <stdbool.h>
#include "scheduler.h"

typedef atomic_flag spinlock_t;

struct thread;
typedef struct thread thread_t;

struct lock_waiting_thread;
typedef struct lock_waiting_thread lock_waiting_thread_t;

typedef struct {
    spinlock_t lock;
    uint32_t cnt;
    lock_waiting_thread_t* wait_queue;
} semaphore_t;

typedef struct {
    spinlock_t lock;
    lock_waiting_thread_t* wait_queue;
    thread_t* owner;
}mutex_t;

#define LOCK_TIMEOUT_INF (uint64_t)-1

/**
 * spinlock_acquire_irq:
 * Acquires a spinlock while also disabling interrupts
 * @param lock The spinlock to acquire
 * @return the saved eflags register
 */
uint32_t spinlock_acquire_irq(spinlock_t* lock);

/**
 * spinlock_release_irq:
 * Releases a spinlock while restoring the irq state
 * @param lock The spinlock to release
 * @param flags The eflags register to restore
 */
void spinlock_release_irq(spinlock_t* lock, uint32_t flags);

/**
 * spinlock_acquire:
 * Tries to acquire a spinlock and switches to another task if currently locked
 * @param lock The spinlock to acquire
 */
void spinlock_acquire(spinlock_t* lock);
/**
 * spinlock_release:
 * Releases a spinlock
 * @param lock The spinlock to release
 */
void spinlock_release(spinlock_t* lock);

/**
 * spinlock_init:
 * Initializes a spinlock
 * @param lock The spinlock to initialize
 */
void spinlock_init(spinlock_t* lock);

/**
 * mutex_wait:
 * Waits for a mutex to become available
 * @param mutex The mutex to wait on
 * @param timeout The timeout in milliseconds
 * @return true if the mutex was acquired, false if timed out
 */
bool mutex_wait(mutex_t* mutex,uint64_t timeout);

/**
 * mutex_signal:
 * Frees a mutex
 * @param mutex The mutex to signal
 */
void mutex_signal(mutex_t* mutex);

/**
 * mutex_init:
 * Initializes a mutex
 * @param mutex The mutex to initialize
 */
void mutex_init(mutex_t* mutex);


/**
 * semaphore_wait:
 * Waits for a semaphore to become available
 * @param sem The semaphore to wait on
 * @param timeout The timeout in milliseconds
 * @return true if the semaphore was acquired, false if timed out
 */
bool semaphore_wait(semaphore_t* sem, uint64_t timeout);

/**
 * semaphore_signal:
 * Signals a semaphore
 * @param sem The semaphore to signal
 */
void semaphore_signal(semaphore_t* sem);

/**
 * semaphore_init:
 * Initializes a semaphore
 * @param sem The semaphore to initialize
 * @param n The initial count of the semaphore
 */
void semaphore_init(semaphore_t* sem, uint32_t n);
#endif