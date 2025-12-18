#ifndef H_CONTEXT
#define H_CONTEXT

#include "lib/memory.h"

typedef struct AppContext {
  ArenaAllocator arena;
  u8 num_threads;
} AppContext;

WASM_EXPORT(app_ctx_current) HZ_ENGINE_API AppContext *app_ctx_current();
WASM_EXPORT(app_ctx_set) HZ_ENGINE_API void app_ctx_set(AppContext *ctx);

#endif
