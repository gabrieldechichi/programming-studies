#include "context.h"
#include "lib/thread_context.h"
#include "lib/typedefs.h"
#include "os/os.h"
#include "lib/math.h"
#include "cube.h"
#include "renderer.h"
#include "camera.h"
#include "app.h"
#include "shaders/cube_vs.h"
#include "shaders/cube_fs.h"

#define VERTEX_STRIDE 40
#define VERTEX_NORMAL_OFFSET 12
#define VERTEX_COLOR_OFFSET 24

#define MAX_CUBES 64

typedef struct {
  Camera camera;
  GpuMesh_Handle cube_mesh;
  Material_Handle cube_material;
  mat4 cube_matrices[MAX_CUBES];
} GameState;

global GameState g_state;

void app_init(AppMemory *memory) {
  if (!is_main_thread()) {
    return;
  }

  AppContext *app_ctx = app_ctx_current();

  g_state.camera = camera_init(VEC3(0, 5, 30), VEC3(0, 0, 0), 45.0f);

  renderer_init(&app_ctx->arena, app_ctx->num_threads, (u32)memory->canvas_width, (u32)memory->canvas_height);

  g_state.cube_mesh = renderer_upload_mesh(&(MeshDesc){
      .vertices = cube_vertices,
      .vertex_size = sizeof(cube_vertices),
      .indices = cube_indices,
      .index_size = sizeof(cube_indices),
      .index_count = CUBE_INDEX_COUNT,
      .index_format = GPU_INDEX_FORMAT_U16,
  });

  g_state.cube_material = renderer_create_material(&(MaterialDesc){
      .shader_desc =
          (GpuShaderDesc){
              .vs_code = (const char *)cube_vs,
              .fs_code = (const char *)cube_fs,
              .uniform_blocks = FIXED_ARRAY_DEFINE(GpuUniformBlockDesc,
                  {.stage = GPU_STAGE_VERTEX, .size = sizeof(GlobalUniforms), .binding = 0},
                  {.stage = GPU_STAGE_VERTEX, .size = sizeof(vec4), .binding = 1},
              ),
          },
      .vertex_layout = {
          .stride = VERTEX_STRIDE,
          .attrs = FIXED_ARRAY_DEFINE(GpuVertexAttr,
              {GPU_VERTEX_FORMAT_FLOAT3, 0, 0},
              {GPU_VERTEX_FORMAT_FLOAT3, VERTEX_NORMAL_OFFSET, 1},
              {GPU_VERTEX_FORMAT_FLOAT4, VERTEX_COLOR_OFFSET, 2},
          ),
      },
      .primitive = GPU_PRIMITIVE_TRIANGLES,
      .depth_test = true,
      .depth_write = true,
      .properties = FIXED_ARRAY_DEFINE(MaterialPropertyDesc,
          {.name = "color", .type = MAT_PROP_VEC4, .binding = 1},
      ),
  });

  material_set_vec4(g_state.cube_material, "color", (vec4){0.2f, 0.6f, 1.0f, 1.0f});

  LOG_INFO("Initialization complete. % cubes (one per thread).", FMT_UINT(app_ctx->num_threads));
}

void app_update_and_render(AppMemory *memory) {
  ThreadContext *tctx = tctx_current();
  u32 thread_idx = tctx->thread_idx;

  if (thread_idx < tctx->thread_count) {
    mat4 *model = &g_state.cube_matrices[thread_idx];
    mat4_identity(*model);

    f32 spacing = 3.0f;
    f32 offset = (tctx->thread_count - 1) * spacing * 0.5f;
    vec3 pos = {thread_idx * spacing - offset, 0.0f, 0.0f};
    mat4_translate(*model, pos);

    f32 angle = memory->total_time + thread_idx * 0.5f;
    mat4_rotate(*model, angle, VEC3(0, 1, 0));
    mat4_rotate(*model, angle * 0.7f, VEC3(1, 0, 0));
  }

  lane_sync();

  if (is_main_thread()) {
    camera_update(&g_state.camera, memory->canvas_width, memory->canvas_height);

    renderer_begin_frame(g_state.camera.view, g_state.camera.proj,
                         (GpuColor){0.1f, 0.1f, 0.15f, 1.0f}, memory->total_time);

    for (u32 i = 0; i < tctx->thread_count; i++) {
      renderer_draw_mesh(g_state.cube_mesh, g_state.cube_material, g_state.cube_matrices[i]);
    }

    renderer_end_frame();
  }
}
