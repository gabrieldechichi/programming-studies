#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include "unistd.h"
#include "typedefs.h"
#include "thread.h"

#include "pthread_barrier.c"
#include "thread.c"

#define ARRAY_SIZE 1000000000
// #define ARRAY_SIZE 1000000

void entrypoint() {
  u64 array_size = 0;
  i64 *array = NULL;

  if (is_main_thread()) {
    array_size = ARRAY_SIZE;
    array = malloc(array_size * sizeof(i64));
    for (u64 i = 0; i < array_size; i++) {
      array[i] = i + 1; // 1, 2, 3, ...
    }
  }

  lane_sync_u64(0, &array_size);
  lane_sync_u64(0, &array);

  Range_u64 range = lane_range(array_size);

  printf("Thread %d start %lld end %lld\n", tctx_current()->thread_idx,
         range.min, range.max);

  // Compute sum
  i64 sum = 0;
  lane_sync_u64(0, &sum);

  i64 lane_sum = 0;
  for (u64 i = range.min; i < range.max; i++) {
    lane_sum += array[i];
  }

  atomic_add(&sum, lane_sum);

  lane_sync_u64(0, &sum);

  if (is_main_thread()) {
    printf("Sum: %lld\n", sum);
  }
}

void *entrypoint_internal(void *arg) {
  ThreadContext *ctx = (ThreadContext *)arg;
  tctx_set_current(ctx);

  entrypoint();
  return NULL;
}

int main(void) {
  const u8 thread_mult = 1;
  i8 core_count = os_core_count();
  u8 thread_count = core_count * thread_mult;

  printf("Core count %d Thread count %d\n", core_count, thread_count);

  pthread_t *threads = malloc(thread_count * sizeof(pthread_t));
  ThreadContext *thread_ctx_arr = malloc(thread_count * sizeof(ThreadContext));
  pthread_barrier_t barrier;
  pthread_barrier_init(&barrier, NULL, thread_count);

  u64 broadcast_memory = 0;
  for (u8 i = 0; i < thread_count; i++) {
    thread_ctx_arr[i] = (ThreadContext){
        .thread_idx = i,
        .thread_count = thread_count,
        .barrier = &barrier,
        .broadcast_memory = &broadcast_memory,
    };
    pthread_create(&threads[i], NULL, entrypoint_internal, &thread_ctx_arr[i]);
  }

  for (u8 i = 0; i < thread_count; i++) {
    pthread_join(threads[i], NULL);
  }

  return 0;
}
