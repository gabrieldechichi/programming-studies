// Include lib .c files directly
#include "context.h"
#include "lib/thread_context.h"
#include "lib/typedefs.h"
#include "os/os.h"
#include "lib/math.h"
#include "cube.h"
#include "renderer.h"
#include "camera.h"
#include "app.h"

void app_init(AppMemory *memory) {
  UNUSED(memory);
  ThreadContext *ctx = tctx_current();
  LOG_INFO("Init from thread: %", FMT_UINT(ctx->thread_idx));

  if (is_main_thread()) {

    AppContext *app_ctx = app_ctx_current();
    LOG_INFO("Main thread has access to app_ctx: %",
             FMT_UINT(app_ctx->num_threads));
  }
}

void app_update_and_render(AppMemory *memory) {
  ThreadContext *tctx = tctx_current();
  u32 thread_idx = tctx->thread_idx;

  local_persist thread_local f32 timer = 0.0f;
  timer += memory->dt;
  if (timer > 0.4f) {
    LOG_INFO("update from thread: %", FMT_UINT(tctx->thread_idx));
    timer = 0;
  }
}
