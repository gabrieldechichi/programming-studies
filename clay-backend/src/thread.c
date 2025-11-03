#include "thread.h"
#include "typedefs.h"

thread_static ThreadContext *__thread_ctx;
ThreadContext *tctx_current() { return __thread_ctx; }
void tctx_set(ThreadContext *ctx) { __thread_ctx = ctx; }
