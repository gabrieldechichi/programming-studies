#include "sokol/sokol_app.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"
#include "sokol/sokol_log.h"
#include "../shaders/triangle.h"

// Application state
typedef enum {
  APP_STATE_READY,
  APP_STATE_RENDERING
} app_state_t;

static sg_pass_action pass_action;
static sg_pipeline pip;
static sg_bindings bind;
static app_state_t app_state = APP_STATE_READY;

// Triangle vertices with position and color
static float vertices[] = {
    // positions      colors
    0.0f,  0.5f,  1.0f, 0.0f, 0.0f, 1.0f, // top vertex (red)
    0.5f,  -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, // bottom right (green)
    -0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 1.0f  // bottom left (blue)
};

// Shader creation function
static void create_shader_pipeline(void) {
  // Create shader using compiled header
  sg_shader shd = sg_make_shader(triangle_shader_desc(sg_query_backend()));

  // Create pipeline
  pip = sg_make_pipeline(&(sg_pipeline_desc){
      .shader = shd,
      .layout = {.attrs = {[ATTR_triangle_position].format = SG_VERTEXFORMAT_FLOAT2,
                           [ATTR_triangle_color].format = SG_VERTEXFORMAT_FLOAT4}},
      .label = "triangle-pipeline"});

  app_state = APP_STATE_RENDERING;
}

static void init(void) {
  sg_setup(&(sg_desc){
      .environment = sglue_environment(),
      .logger.func = slog_func,
  });

  // Create vertex buffer
  bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
      .data = SG_RANGE(vertices), .label = "triangle-vertices"});

  // Create shader pipeline immediately
  create_shader_pipeline();

  // Set up clear color (black background)
  pass_action =
      (sg_pass_action){.colors[0] = {.load_action = SG_LOADACTION_CLEAR,
                                     .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}}};
}

static void frame(void) {
  sg_begin_pass(
      &(sg_pass){.action = pass_action, .swapchain = sglue_swapchain()});

  // Render triangle (always ready now)
  if (app_state == APP_STATE_RENDERING) {
    sg_apply_pipeline(pip);
    sg_apply_bindings(&bind);
    sg_draw(0, 3, 1);
  }

  sg_end_pass();
  sg_commit();
}

static void cleanup(void) {
  sg_shutdown();
}

sapp_desc sokol_main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;
  return (sapp_desc){
      .init_cb = init,
      .frame_cb = frame,
      .cleanup_cb = cleanup,
      .width = 800,
      .height = 600,
      .window_title = "Sokol Window",
      .icon.sokol_default = true,
      .logger.func = slog_func,
  };
}
