#ifndef THREAD_H
#define THREAD_H

#include "typedefs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { u64 v[1]; } Thread;
typedef struct { u64 v[1]; } Mutex;
typedef struct { u64 v[1]; } RWMutex;
typedef struct { u64 v[1]; } CondVar;
typedef struct { u64 v[1]; } Semaphore;
typedef struct { u64 v[1]; } Barrier;

typedef void (*ThreadFunc)(void *);

Thread thread_launch(ThreadFunc fn, void *arg);
b32 thread_join(Thread t, u64 timeout_us);
void thread_detach(Thread t);
void thread_set_name(Thread t, const char *name);

Mutex mutex_alloc(void);
void mutex_release(Mutex m);
void mutex_take(Mutex m);
void mutex_drop(Mutex m);

RWMutex rw_mutex_alloc(void);
void rw_mutex_release(RWMutex m);
void rw_mutex_take_r(RWMutex m);
void rw_mutex_take_w(RWMutex m);
void rw_mutex_drop_r(RWMutex m);
void rw_mutex_drop_w(RWMutex m);

CondVar cond_var_alloc(void);
void cond_var_release(CondVar cv);
b32 cond_var_wait(CondVar cv, Mutex m, u64 timeout_us);
void cond_var_signal(CondVar cv);
void cond_var_broadcast(CondVar cv);

Semaphore semaphore_alloc(i32 initial_count);
void semaphore_release(Semaphore s);
void semaphore_take(Semaphore s);
void semaphore_drop(Semaphore s);

Barrier barrier_alloc(u32 count);
void barrier_release(Barrier b);
void barrier_wait(Barrier b);

#define MutexScope(m) DeferLoop(mutex_take(m), mutex_drop(m))
#define RWMutexScopeR(m) DeferLoop(rw_mutex_take_r(m), rw_mutex_drop_r(m))
#define RWMutexScopeW(m) DeferLoop(rw_mutex_take_w(m), rw_mutex_drop_w(m))

#ifdef __cplusplus
}
#endif

#endif
