#include "thread.h"

#include <errno.h>

// Include macOS pthread_barrier implementation
#ifdef __APPLE__
#include "pthread_barrier.c"
#endif

#ifdef _WIN32

//- Windows Thread Implementation

// Windows thread wrapper that converts between conventions
static DWORD __stdcall thread_func_wrapper(void *arg) {
  ThreadFunc func = ((void**)arg)[0];
  void *user_arg = ((void**)arg)[1];
  free(arg);
  func(user_arg);
  return 0;
}

Thread thread_create(ThreadFunc func, void *arg) {
  Thread thread;
  // Package both the function and argument together
  void **wrapper_arg = malloc(2 * sizeof(void*));
  wrapper_arg[0] = (void*)func;
  wrapper_arg[1] = arg;

  thread.handle = CreateThread(
    NULL,                    // Security attributes
    0,                       // Stack size (0 = default)
    thread_func_wrapper,     // Wrapper with correct convention
    wrapper_arg,             // Packaged arguments
    0,                       // Creation flags (0 = start immediately)
    NULL                     // Thread ID (don't need it)
  );
  return thread;
}

void thread_join(Thread thread) {
  if (thread.handle != NULL) {
    WaitForSingleObject(thread.handle, INFINITE);
    CloseHandle(thread.handle);
  }
}

//- Windows Barrier Implementation using SYNCHRONIZATION_BARRIER

int barrier_init(Barrier *barrier, unsigned int count) {
  if (count == 0) {
    errno = EINVAL;
    return -1;
  }

  BOOL result = InitializeSynchronizationBarrier(&barrier->sb, count, -1);
  if (!result) {
    errno = EINVAL;
    return -1;
  }

  return 0;
}

int barrier_destroy(Barrier *barrier) {
  DeleteSynchronizationBarrier(&barrier->sb);
  return 0;
}

int barrier_wait(Barrier *barrier) {
  BOOL result = EnterSynchronizationBarrier(&barrier->sb, 0);
  // Windows returns TRUE if this is the last thread, which mirrors
  // pthread_barrier_wait returning PTHREAD_BARRIER_SERIAL_THREAD
  return result ? 1 : 0;
}

#elif defined(__APPLE__)

//- macOS Thread Implementation using pthreads

Thread thread_create(ThreadFunc func, void *arg) {
  Thread thread;
  pthread_create(&thread.handle, NULL, func, arg);
  return thread;
}

void thread_join(Thread thread) {
  pthread_join(thread.handle, NULL);
}

//- macOS Barrier Implementation using pthread_barrier.c (mutex+condvar)
// The implementation is in pthread_barrier.c

int barrier_init(Barrier *barrier, unsigned int count) {
  return pthread_barrier_init(&barrier->barrier, NULL, count);
}

int barrier_destroy(Barrier *barrier) {
  return pthread_barrier_destroy(&barrier->barrier);
}

int barrier_wait(Barrier *barrier) {
  return pthread_barrier_wait(&barrier->barrier);
}

#else

//- Linux Thread Implementation using pthreads

Thread thread_create(ThreadFunc func, void *arg) {
  Thread thread;
  pthread_create(&thread.handle, NULL, func, arg);
  return thread;
}

void thread_join(Thread thread) {
  pthread_join(thread.handle, NULL);
}

//- Linux Barrier Implementation using native pthread_barrier

int barrier_init(Barrier *barrier, unsigned int count) {
  return pthread_barrier_init(&barrier->barrier, NULL, count);
}

int barrier_destroy(Barrier *barrier) {
  return pthread_barrier_destroy(&barrier->barrier);
}

int barrier_wait(Barrier *barrier) {
  return pthread_barrier_wait(&barrier->barrier);
}

#endif
