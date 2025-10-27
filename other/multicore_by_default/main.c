#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include "pthread_barrier.h"
#include "unistd.h"
#include "pthread_barrier.c"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef struct ThreadContext {
  u8 thread_idx;
  u8 thread_count;
  pthread_barrier_t *barrier;
} ThreadContext;

typedef struct AppContext {
  u64 array_size;
  i64 *array;
} AppContext;

static AppContext *app_ctx = NULL;

void entrypoint(ThreadContext *ctx) {

  if (ctx->thread_idx == 0) {
    app_ctx->array_size = 100000000;
    app_ctx->array = malloc(app_ctx->array_size * sizeof(i64));
    for (u64 i = 0; i < app_ctx->array_size; i++) {
      app_ctx->array[i] = i + 1; // 1, 2, 3, ...
    }
  }
  pthread_barrier_wait(ctx->barrier);

  // Compute sum
  i64 sum = 0;
  for (u64 i = 0; i < app_ctx->array_size; i++) {
    sum += app_ctx->array[i];
  }

  printf("Sum: %lld\n", (u64)sum);

  pthread_barrier_wait(ctx->barrier);
  if (ctx->thread_idx == 0) {
    free(app_ctx->array);
  }
}

void *entrypoint_internal(void *arg) {
  ThreadContext *ctx = (ThreadContext *)arg;
  entrypoint(ctx);
  return NULL;
}

i8 os_core_count() { return (i8)sysconf(_SC_NPROCESSORS_ONLN); }

int main(void) {
  const u8 thread_mult = 1;
  i8 core_count = os_core_count();
  u8 thread_count = core_count * thread_mult;

  printf("Core count %d Thread count %d\n", core_count, thread_count);

  pthread_t *threads = malloc(thread_count * sizeof(pthread_t));
  ThreadContext *ctx_arr = malloc(thread_count * sizeof(ThreadContext));
  pthread_barrier_t barrier;
  pthread_barrier_init(&barrier, NULL, thread_count);

  app_ctx = calloc(1, sizeof(AppContext));

  for (u8 i = 0; i < thread_count; i++) {
    ctx_arr[i] = (ThreadContext){
        .thread_idx = i,
        .thread_count = thread_count,
        .barrier = &barrier,
    };
    pthread_create(&threads[i], NULL, entrypoint_internal, &ctx_arr[i]);
  }

  for (u8 i = 0; i < thread_count; i++) {
    pthread_join(threads[i], NULL);
  }

  return 0;
}
