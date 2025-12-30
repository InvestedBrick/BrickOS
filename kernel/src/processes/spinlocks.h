#ifndef INCLUDE_SPINLOCKS_H
#define INCLUDE_SPINLOCKS_H

#include <stdatomic.h>
#include <stdint.h>
#include <stdbool.h>

typedef atomic_flag Spinlock;

typedef struct {
    Spinlock lock;
    int32_t cnt;
} Semaphore_t;

typedef struct {
    Spinlock lock;
    bool free;
}Mutex_t;

/**
 * spinlock_aquire:
 * Tries to aquire a spinlock and switches to another task if currently locked
 * @param lock The spinlock to acquire
 */
void spinlock_aquire(Spinlock* lock);
/**
 * spinlock_release:
 * Releases a spinlock
 * @param lock The spinlock to release
 */
void spinlock_release(Spinlock* lock);

/**
 * spinlock_init:
 * Initializes a spinlock
 * @param lock The spinlock to initialize
 */
void spinlock_init(Spinlock* lock);

/**
 * mutex_wait:
 * Waits for a mutex to become available
 * @param mutex The mutex to wait on
 * @param timeout The timeout in milliseconds
 * @return true if the mutex was acquired, false if timed out
 */
bool mutex_wait(Mutex_t* mutex,uint32_t timeout);

/**
 * mutex_signal:
 * Frees a mutex
 * @param mutex The mutex to signal
 */
void mutex_signal(Mutex_t* mutex);

/**
 * mutex_init:
 * Initializes a mutex
 * @param mutex The mutex to initialize
 */
void mutex_init(Mutex_t* mutex);


/**
 * semaphore_wait:
 * Waits for a semaphore to become available
 * @param sem The semaphore to wait on
 * @param timeout The timeout in milliseconds
 * @return true if the semaphore was acquired, false if timed out
 */
bool semaphore_wait(Semaphore_t* sem, uint32_t timeout);

/**
 * semaphore_signal:
 * Signals a semaphore
 * @param sem The semaphore to signal
 */
void semaphore_signal(Semaphore_t* sem);

/**
 * semaphore_init:
 * Initializes a semaphore
 * @param sem The semaphore to initialize
 * @param n The initial count of the semaphore
 */
void semaphore_init(Semaphore_t* sem, int32_t n);
#endif