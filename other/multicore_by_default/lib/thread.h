#ifndef THREAD_H
#define THREAD_H

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include "pthread_barrier.h"
#else
#include <pthread.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

//- Thread Types

typedef struct Thread Thread;
struct Thread {
#ifdef _WIN32
  void *handle;  // HANDLE on Windows
#else
  pthread_t handle;
#endif
};

typedef void* (*ThreadFunc)(void *arg);

//- Thread Functions

Thread thread_create(ThreadFunc func, void *arg);
void thread_join(Thread thread);

//- Barrier Types

typedef struct Barrier Barrier;
struct Barrier {
#ifdef _WIN32
  SYNCHRONIZATION_BARRIER sb;
#elif defined(__APPLE__)
  pthread_barrier_t barrier;
#else
  pthread_barrier_t barrier;
#endif
};

//- Barrier Functions

int barrier_init(Barrier *barrier, unsigned int count);
int barrier_destroy(Barrier *barrier);
int barrier_wait(Barrier *barrier);

#ifdef __cplusplus
}
#endif

#endif // THREAD_H
