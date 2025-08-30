#include "sokol/sokol_app.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"
#include "sokol/sokol_log.h"

static sg_pass_action pass_action;
static sg_pipeline pip;
static sg_bindings bind;

// Triangle vertices with position and color
static float vertices[] = {
    // positions      colors
    0.0f,  0.5f,  1.0f, 0.0f, 0.0f, 1.0f, // top vertex (red)
    0.5f,  -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, // bottom right (green)
    -0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 1.0f  // bottom left (blue)
};

// Simple triangle shaders for Metal backend
static const char *vs_metal =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct vs_in {\n"
    "  float2 pos [[attribute(0)]];\n"
    "  float4 color [[attribute(1)]];\n"
    "};\n"
    "struct vs_out {\n"
    "  float4 pos [[position]];\n"
    "  float4 color;\n"
    "};\n"
    "vertex vs_out vs_main(vs_in inp [[stage_in]]) {\n"
    "  vs_out outp;\n"
    "  outp.pos = float4(inp.pos, 0.0, 1.0);\n"
    "  outp.color = inp.color;\n"
    "  return outp;\n"
    "}\n";

static const char *fs_metal =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "fragment float4 fs_main(float4 color [[stage_in]]) {\n"
    "  return color;\n"
    "}\n";

static void init(void) {
  sg_setup(&(sg_desc){
      .environment = sglue_environment(),
      .logger.func = slog_func,
  });

  // Create vertex buffer
  bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
      .data = SG_RANGE(vertices), .label = "triangle-vertices"});

  // Create shader for Metal backend
  sg_shader shd = sg_make_shader(&(sg_shader_desc){
      .vertex_func = {.source = vs_metal, .entry = "vs_main"},
      .fragment_func = {.source = fs_metal, .entry = "fs_main"},
      .label = "triangle-shader"});

  // Create pipeline
  pip = sg_make_pipeline(&(sg_pipeline_desc){
      .shader = shd,
      .layout = {.attrs = {[0].format = SG_VERTEXFORMAT_FLOAT2,
                           [1].format = SG_VERTEXFORMAT_FLOAT4}},
      .label = "triangle-pipeline"});

  // Set up clear color (black background)
  pass_action =
      (sg_pass_action){.colors[0] = {.load_action = SG_LOADACTION_CLEAR,
                                     .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}}};
}

static void frame(void) {
  sg_begin_pass(
      &(sg_pass){.action = pass_action, .swapchain = sglue_swapchain()});

  sg_apply_pipeline(pip);
  sg_apply_bindings(&bind);
  sg_draw(0, 3, 1);

  sg_end_pass();
  sg_commit();
}

static void cleanup(void) { sg_shutdown(); }

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
