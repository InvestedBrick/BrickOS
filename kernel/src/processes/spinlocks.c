#include "spinlocks.h"
#include "scheduler.h"
#include "../tables/interrupts.h"

void spinlock_init(Spinlock* lock){
    atomic_flag_clear(lock);
}

void spinlock_aquire(Spinlock* lock){
    while(atomic_flag_test_and_set_explicit(lock,memory_order_acquire))
        invoke_scheduler();
}

void spinlock_release(Spinlock* lock){
    atomic_flag_clear_explicit(lock,memory_order_release);
}

void mutex_init(Mutex_t* mutex){
    spinlock_init(&mutex->lock);
    mutex->free = true;
}

bool mutex_wait(Mutex_t* mutex,uint32_t timeout){
    bool ret = false;
    uint64_t timer_start = ticks;
    while(true){
        if (timeout > 0 && ticks > (timer_start + timeout)) 
            goto just_return;
    
        spinlock_aquire(&mutex->lock);
        if (mutex->free){
            mutex->free = false;
            ret = true;
            break;
        }
        spinlock_release(&mutex->lock);
        invoke_scheduler();
    }

    spinlock_release(&mutex->lock);
just_return:
    return ret;
}
void mutex_signal(Mutex_t* mutex){
    spinlock_aquire(&mutex->lock);
    mutex->free = true;
    spinlock_release(&mutex->lock);
}

void semaphore_init(Semaphore_t* sem, int32_t n){
    spinlock_init(&sem->lock);
    sem->cnt = n;
}

bool semaphore_wait(Semaphore_t* sem, uint32_t timeout){
    bool ret = false;
    uint64_t timer_start = ticks;
    while(true){
        if (timeout > 0 && ticks > (timer_start + timeout)) 
            goto just_return;
        spinlock_aquire(&sem->lock);
        if (sem->cnt > 0){
            sem->cnt--;
            ret = true;
            break;
        }
        spinlock_release(&sem->lock);
        invoke_scheduler();
    }
release_lock:
    spinlock_release(&sem->lock);
just_return:
    return ret;
}
void semaphore_signal(Semaphore_t* sem){
    spinlock_aquire(&sem->lock);
    sem->cnt++;
    spinlock_release(&sem->lock);
}