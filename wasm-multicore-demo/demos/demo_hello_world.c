#include "app.h"
#include "lib/typedefs.h"

void app_init(AppMemory *memory) {
    ThreadContext *tctx = tctx_current();
    LOG_INFO("Hello World init from thread: %", FMT_UINT(tctx->thread_idx));
}

void app_update_and_render(AppMemory *memory) {
  local_persist thread_local f32 timer = 0;

  timer += memory->dt;
  if (timer > 0.99) {

    ThreadContext *tctx = tctx_current();
    timer = 0;
    LOG_INFO("Hello World update from thread: %", FMT_UINT(tctx->thread_idx));
  }
}
