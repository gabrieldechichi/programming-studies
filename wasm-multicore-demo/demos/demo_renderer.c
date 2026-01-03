#include "context.h"
#include "lib/thread_context.h"
#include "lib/typedefs.h"
#include "os/os.h"
#include "renderer.h"
#include "app.h"

void app_init(AppMemory *memory) {
  if (!is_main_thread()) {
    return;
  }

  AppContext *app_ctx = app_ctx_current();
  renderer_init(&app_ctx->arena, app_ctx->num_threads, (u32)memory->canvas_width, (u32)memory->canvas_height, 4);

  LOG_INFO("Renderer demo initialized");
}

void app_update_and_render(AppMemory *memory) {
  if (!is_main_thread()) {
    return;
  }

  mat4 view, proj;
  mat4_identity(view);
  mat4_identity(proj);

  renderer_begin_frame(view, proj, (GpuColor){0.2f, 0.3f, 0.4f, 1.0f}, memory->total_time);
  renderer_end_frame();
}
