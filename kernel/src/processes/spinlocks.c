#include "spinlocks.h"
#include "scheduler.h"
#include "../tables/interrupts.h"
#include "../memory/kmalloc.h"

void spinlock_init(spinlock_t* lock){
    atomic_flag_clear(lock);
}

void spinlock_acquire(spinlock_t* lock){
    while(atomic_flag_test_and_set_explicit(lock,memory_order_acquire))
        invoke_scheduler();
}

void spinlock_release(spinlock_t* lock){
    atomic_flag_clear_explicit(lock,memory_order_release);
}

void mutex_init(mutex_t* mutex){
    spinlock_init(&mutex->lock);
    mutex->free = true;
    mutex->wait_queue = nullptr;
}

lock_waiting_thread_t* block_current_thread(lock_waiting_thread_t** wait_queue, uint64_t timeout, spinlock_t* wait_lock){
    thread_t* curr_thread = get_current_thread();
    lock_waiting_thread_t* wthread = (lock_waiting_thread_t*)kmalloc(sizeof(lock_waiting_thread_t));
    wthread->next = nullptr;
    wthread->signaled = false;
    wthread->thread = curr_thread;
    
    uint64_t timeout_ticks = MS_TO_TICKS(timeout);
    if (timeout == LOCK_TIMEOUT_INF) timeout_ticks = THREAD_ETERNAL_SLEEP;

    add_sleeping_thread(curr_thread,timeout_ticks);
    spinlock_acquire(wait_lock);
    lock_waiting_thread_t* curr = *wait_queue;
    if (!curr) *wait_queue = wthread;
    else{
        while(curr->next) curr = curr->next;
        curr->next = wthread;
    }
    spinlock_release(wait_lock);

    return wthread;
}

void unlink_waiting_thread(lock_waiting_thread_t** wait_queue, lock_waiting_thread_t* target){
    if (!target) return;
    lock_waiting_thread_t* prev = *wait_queue;

    if (prev == target) {
        *wait_queue = prev->next;
    }else{
        while(prev->next && prev->next != target) prev = prev->next;
        if (prev->next){
            prev->next = prev->next->next;
        }
    }
}

bool mutex_wait(mutex_t* mutex,uint64_t timeout){
    spinlock_acquire(&mutex->lock);
    if (mutex->free){
        mutex->free = false;
        mutex->owner = get_current_thread();
        spinlock_release(&mutex->lock);
        return true;
    }
    spinlock_release(&mutex->lock);
    if (!timeout) return false; 

    lock_waiting_thread_t* wthread = block_current_thread(&mutex->wait_queue,timeout,&mutex->lock);
    invoke_scheduler();

    spinlock_acquire(&mutex->lock);
    uint8_t signaled = wthread->signaled;
    if (!signaled) {
        unlink_waiting_thread(&mutex->wait_queue, wthread);
    }
    spinlock_release(&mutex->lock);
    kfree(wthread);

    return signaled;
}
void mutex_signal(mutex_t* mutex){
    spinlock_acquire(&mutex->lock);
    thread_t* curr_thread = get_current_thread();
    // kinda pointless since we have a struct where the members could just be changed but we still prevent accidental invalid signals
    if (mutex->owner != curr_thread) {
        spinlock_release(&mutex->lock);
        return;
    }
    if (mutex->wait_queue){
        lock_waiting_thread_t* wthread = mutex->wait_queue;
        mutex->wait_queue = wthread->next;
        mutex->owner = wthread->thread;
        wthread->signaled = true;
        wakeup_thread(wthread->thread);
    }else{
        mutex->free = true;
        mutex->owner = nullptr;
    }

    spinlock_release(&mutex->lock);
}

void semaphore_init(semaphore_t* sem, uint32_t n){
    spinlock_init(&sem->lock);
    sem->cnt = n;
    sem->wait_queue = nullptr;
}

bool semaphore_wait(semaphore_t* sem, uint64_t timeout){
    spinlock_acquire(&sem->lock);
    if (sem->cnt > 0){
        sem->cnt--;
        spinlock_release(&sem->lock);
        return true;
    }
    spinlock_release(&sem->lock);
    if (!timeout) return false;

    lock_waiting_thread_t* wthread = block_current_thread(&sem->wait_queue,timeout,&sem->lock);
    invoke_scheduler();
    
    spinlock_acquire(&sem->lock);
    uint8_t signaled = wthread->signaled;
    if (!signaled) {
        unlink_waiting_thread(&sem->wait_queue, wthread);
    }
    spinlock_release(&sem->lock);
    kfree(wthread);

    return signaled;
}
void semaphore_signal(semaphore_t* sem){
    spinlock_acquire(&sem->lock);
    if (sem->wait_queue){
        lock_waiting_thread_t* wthread = sem->wait_queue;
        sem->wait_queue = wthread->next;
        wthread->signaled = true;
        wakeup_thread(wthread->thread);
    }else{
        sem->cnt++;
    }
    spinlock_release(&sem->lock);
}