#include "thread.h"
#include "os/os.h"

Thread thread_launch(ThreadFunc fn, void *arg) { return os_thread_launch(fn, arg); }
b32 thread_join(Thread t, u64 timeout_us) { return os_thread_join(t, timeout_us); }
void thread_detach(Thread t) { os_thread_detach(t); }

Mutex mutex_alloc(void) { return os_mutex_alloc(); }
void mutex_release(Mutex m) { os_mutex_release(m); }
void mutex_take(Mutex m) { os_mutex_take(m); }
void mutex_drop(Mutex m) { os_mutex_drop(m); }

RWMutex rw_mutex_alloc(void) { return os_rw_mutex_alloc(); }
void rw_mutex_release(RWMutex m) { os_rw_mutex_release(m); }
void rw_mutex_take_r(RWMutex m) { os_rw_mutex_take_r(m); }
void rw_mutex_drop_r(RWMutex m) { os_rw_mutex_drop_r(m); }
void rw_mutex_take_w(RWMutex m) { os_rw_mutex_take_w(m); }
void rw_mutex_drop_w(RWMutex m) { os_rw_mutex_drop_w(m); }

CondVar cond_var_alloc(void) { return os_cond_var_alloc(); }
void cond_var_release(CondVar cv) { os_cond_var_release(cv); }
b32 cond_var_wait(CondVar cv, Mutex m, u64 timeout_us) { return os_cond_var_wait(cv, m, timeout_us); }
void cond_var_signal(CondVar cv) { os_cond_var_signal(cv); }
void cond_var_broadcast(CondVar cv) { os_cond_var_broadcast(cv); }

Semaphore semaphore_alloc(i32 initial_count) { return os_semaphore_alloc(initial_count); }
void semaphore_release(Semaphore s) { os_semaphore_release(s); }
void semaphore_take(Semaphore s) { os_semaphore_take(s); }
void semaphore_drop(Semaphore s) { os_semaphore_drop(s); }

Barrier barrier_alloc(u32 count) { return os_barrier_alloc(count); }
void barrier_release(Barrier b) { os_barrier_release(b); }
void barrier_wait(Barrier b) { os_barrier_wait(b); }
