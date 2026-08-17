#include "spinlocks.h"
#include "scheduler.h"
#include "../tables/interrupts.h"
#include "../memory/kmalloc.h"

void spinlock_init(spinlock_t* lock){
    atomic_flag_clear(lock);
}

void spinlock_acquire(spinlock_t* lock){
    while(atomic_flag_test_and_set_explicit(lock,memory_order_acquire))
        yield();
}

void spinlock_release(spinlock_t* lock){
    atomic_flag_clear_explicit(lock,memory_order_release);
}

void mutex_init(mutex_t* mutex){
    spinlock_init(&mutex->lock);
    mutex->wait_queue = nullptr;
    mutex->owner = nullptr;
}

lock_waiting_thread_t* block_current_thread(lock_waiting_thread_t** wait_queue, uint64_t timeout){
    thread_t* curr_thread = get_current_thread();

    curr_thread->lock_wthread.next = nullptr;
    curr_thread->lock_wthread.wait_state = WAITING;
    curr_thread->lock_wthread.thread = curr_thread;
    
    lock_waiting_thread_t* curr = *wait_queue;
    if (!curr) *wait_queue = &curr_thread->lock_wthread;
    else{
        while(curr->next) curr = curr->next;
        curr->next = &curr_thread->lock_wthread;
    }
    
    uint64_t timeout_ticks = MS_TO_TICKS(timeout);
    if (timeout == LOCK_TIMEOUT_INF) timeout_ticks = THREAD_ETERNAL_SLEEP;

    add_sleeping_thread(curr_thread,timeout_ticks);

    return &curr_thread->lock_wthread;
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
    thread_t* curr_thread = get_current_thread();
    if (!mutex->owner){
        mutex->owner = curr_thread;
        spinlock_release(&mutex->lock);
        return true;
    }
    
    if (!timeout || mutex->owner == curr_thread) {
        spinlock_release(&mutex->lock);    
        return false; 
    }

    lock_waiting_thread_t* wthread = block_current_thread(&mutex->wait_queue,timeout);
    spinlock_release(&mutex->lock);
    yield();

    spinlock_acquire(&mutex->lock);
    uint8_t signaled = wthread->wait_state == SIGNALED;
    if (!signaled) {
        unlink_waiting_thread(&mutex->wait_queue, wthread);
    }
    spinlock_release(&mutex->lock);

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
        if (wthread->wait_state == WAITING) wthread->wait_state = SIGNALED;
        wakeup_thread(wthread->thread);
    }else{
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
    if (!timeout) {
        spinlock_release(&sem->lock);
        return false;
    }

    lock_waiting_thread_t* wthread = block_current_thread(&sem->wait_queue,timeout);
    spinlock_release(&sem->lock);
    yield();
    
    spinlock_acquire(&sem->lock);
    uint8_t signaled = wthread->wait_state == SIGNALED;
    if (!signaled) {
        unlink_waiting_thread(&sem->wait_queue, wthread);
    }
    spinlock_release(&sem->lock);

    return signaled;
}

void semaphore_signal(semaphore_t* sem){
    spinlock_acquire(&sem->lock);
    if (sem->wait_queue){
        lock_waiting_thread_t* wthread = sem->wait_queue;
        sem->wait_queue = wthread->next;
        if (wthread->wait_state == WAITING) wthread->wait_state = SIGNALED;
        wakeup_thread(wthread->thread);
    }else{
        sem->cnt++;
    }
    spinlock_release(&sem->lock);
}