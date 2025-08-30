#include "platform.h"
#include "sokol/sokol_app.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"
#include "sokol/sokol_log.h"

// Application state
typedef enum {
  APP_STATE_LOADING,
  APP_STATE_READY,
  APP_STATE_RENDERING
} app_state_t;

static sg_pass_action pass_action;
static sg_pipeline pip;
static sg_bindings bind;
static app_state_t app_state = APP_STATE_LOADING;
static file_handle_t *vertex_shader_handle = NULL;
static file_handle_t *fragment_shader_handle = NULL;

// Triangle vertices with position and color
static float vertices[] = {
    // positions      colors
    0.0f,  0.5f,  1.0f, 0.0f, 0.0f, 1.0f, // top vertex (red)
    0.5f,  -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, // bottom right (green)
    -0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 1.0f  // bottom left (blue)
};

// Shader creation function
static void create_shader_pipeline(void) {
  const char *vs_content = get_file_content(vertex_shader_handle);
  const char *fs_content = get_file_content(fragment_shader_handle);

  if (!vs_content || !fs_content) {
    return;
  }

  // Create shader for Metal backend
  sg_shader shd = sg_make_shader(&(sg_shader_desc){
      .vertex_func = {.source = vs_content, .entry = "vs_main"},
      .fragment_func = {.source = fs_content, .entry = "fs_main"},
      .label = "triangle-shader"});

  // Create pipeline
  pip = sg_make_pipeline(&(sg_pipeline_desc){
      .shader = shd,
      .layout = {.attrs = {[0].format = SG_VERTEXFORMAT_FLOAT2,
                           [1].format = SG_VERTEXFORMAT_FLOAT4}},
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

  // Start loading shader files asynchronously
  vertex_shader_handle = load_file_async("shaders/triangle.vert.metal");
  fragment_shader_handle = load_file_async("shaders/triangle.frag.metal");

  // Set up clear color (black background)
  pass_action =
      (sg_pass_action){.colors[0] = {.load_action = SG_LOADACTION_CLEAR,
                                     .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}}};
}

static void frame(void) {
  // Check if we need to transition states
  if (app_state == APP_STATE_LOADING) {
    if (is_file_ready(vertex_shader_handle) &&
        is_file_ready(fragment_shader_handle)) {
      create_shader_pipeline();
    }
  }

  sg_begin_pass(
      &(sg_pass){.action = pass_action, .swapchain = sglue_swapchain()});

  // Only render triangle if shaders are loaded
  if (app_state == APP_STATE_RENDERING) {
    sg_apply_pipeline(pip);
    sg_apply_bindings(&bind);
    sg_draw(0, 3, 1);
  }

  sg_end_pass();
  sg_commit();
}

static void cleanup(void) {
  free_file_handle(vertex_shader_handle);
  free_file_handle(fragment_shader_handle);
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
