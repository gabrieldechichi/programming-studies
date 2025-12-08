#ifndef OS_H
#define OS_H

#include "lib/typedefs.h"
#include "lib/thread.h"

// Memory
void* os_allocate_memory(size_t size);
void os_free_memory(void* ptr, size_t size);
u8* os_reserve_memory(size_t size);
b32 os_commit_memory(void* ptr, size_t size);

// Threads
Thread os_thread_launch(ThreadFunc fn, void* arg);
b32 os_thread_join(Thread t, u64 timeout_us);
void os_thread_detach(Thread t);
void os_thread_set_name(Thread t, const char* name);

// Mutex
Mutex os_mutex_alloc(void);
void os_mutex_release(Mutex m);
void os_mutex_take(Mutex m);
void os_mutex_drop(Mutex m);

// RWMutex
RWMutex os_rw_mutex_alloc(void);
void os_rw_mutex_release(RWMutex m);
void os_rw_mutex_take_r(RWMutex m);
void os_rw_mutex_drop_r(RWMutex m);
void os_rw_mutex_take_w(RWMutex m);
void os_rw_mutex_drop_w(RWMutex m);

// CondVar
CondVar os_cond_var_alloc(void);
void os_cond_var_release(CondVar cv);
b32 os_cond_var_wait(CondVar cv, Mutex m, u64 timeout_us);
void os_cond_var_signal(CondVar cv);
void os_cond_var_broadcast(CondVar cv);

// Semaphore
Semaphore os_semaphore_alloc(i32 initial_count);
void os_semaphore_release(Semaphore s);
void os_semaphore_take(Semaphore s);
void os_semaphore_drop(Semaphore s);

// Barrier
Barrier os_barrier_alloc(u32 count);
void os_barrier_release(Barrier b);
void os_barrier_wait(Barrier b);

#endif
