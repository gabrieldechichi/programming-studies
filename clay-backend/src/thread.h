#ifndef H_THREAD
#define H_THREAD
#include "memory.h"
typedef struct {
  ArenaAllocator temp_allocator;
} ThreadContext;

ThreadContext *tctx_current();
void tctx_set(ThreadContext *ctx);

#endif
