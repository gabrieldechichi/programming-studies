#ifndef H_RENDERER
#define H_RENDERER

#include "lib/array.h"
#include "lib/math.h"
#include "lib/string.h"
#include "lib/typedefs.h"

#define DISPLAY_SAMPLE_COUNT (4)

// Blendshape uniform structure matching shader layout
#define MAX_BLENDSHAPES 32
typedef struct packed_struct {
  u32 count;
  vec3 _pad1;
  vec4 weights[8];
} BlendshapeParams;

typedef struct {
  union {
    struct {
      f32 r, g, b, a;
    };
    struct {
      f32 x, y, z, w;
    };
    f32 components[4];
  };
} Color;

#define hex_to_rgba_255(hex)                                                   \
  {((hex >> 16) & 0xFF), ((hex >> 8) & 0xFF), (hex & 0xFF), 255}

#define hex_to_rgba(hex)                                                       \
  {(f32)(((hex >> 16) & 0xFF) / 255.0), (f32)(((hex >> 8) & 0xFF) / 255.0),    \
   (f32)((hex & 0xFF) / 255.0), 1.0f}

#define color_from_rgba(r, g, b, a) ((Color){{{(r), (g), (b), (a)}}})
#define color_from_hex(hex) ((Color){{hex_to_rgba(hex)}})
#define color_mul(c, v) color_from_rgba(c.r *v, c.g *v, c.b *v, c.a *v);

// Render command types
typedef enum {
  RENDER_CMD_CLEAR,
  RENDER_CMD_DRAW_MESH,
  RENDER_CMD_DRAW_SKINNED_MESH,
  RENDER_CMD_DRAW_SKYBOX,
  RENDER_CMD_MAX
} RenderCommandType;

// Render command data structures
typedef struct {
  Color color;
} RenderClearCommand;

typedef struct {
  Handle mesh_handle;
  Handle material_handle;
  mat4 model_matrix;
} RenderDrawMeshCommand;

typedef struct {
  Handle mesh_handle;
  Handle material_handle;
  mat4 model_matrix;
  mat4 *joint_transforms;
  u32 num_joints;
  BlendshapeParams *blendshape_params;
} RenderDrawSkinnedMeshCommand;

typedef struct {
  Handle material_handle;
} RenderDrawSkyboxCommand;

// Render command union
typedef struct {
  RenderCommandType type;
  union {
    RenderClearCommand clear;
    RenderDrawMeshCommand draw_mesh;
    RenderDrawSkinnedMeshCommand draw_skinned_mesh;
    RenderDrawSkyboxCommand draw_skybox;
  } data;
} RenderCommand;
slice_define(RenderCommand);

// Maximum number of render commands per frame
#define MAX_RENDER_COMMANDS 256

// Skinned mesh structures
typedef struct {
  Handle mesh_handle;
  Handle material_handle;
} SkinnedSubMesh;
arr_define(SkinnedSubMesh);

typedef struct {
  String32Bytes_Array blendshape_names;
  f32_Array blendshape_weights;
  SkinnedSubMesh_Array submeshes;
} SkinnedMesh;
arr_define(SkinnedMesh);

typedef struct {
  SkinnedMesh_Array meshes;
  mat4_Array joint_matrices;
} SkinnedModel;

// camera
typedef struct {
    vec3 camera_pos;
    float _padding0;
    mat4 view_matrix;
    mat4 projection_matrix;
    mat4 view_proj_matrix;  // view * projection
} CameraUniformBlock;

// skinned mesh
#define MAX_JOINTS 256
#define MAX_JOINTS_PER_VERTEX 4

typedef enum {
  HMOBJ_VER_1 = 1,
} Model3DSerializedVersion;

typedef struct {
  i32 parent_index; // Index of parent joint (-1 if root)
  mat4 inverse_bind_matrix;
  u32_Array children;
} Joint;
arr_define(Joint);

typedef struct {
  // number of vertices in the submesh
  u32 len_vertices;

  // span in bytes of a single vertex
  u32 vertex_stride;
  // len of f32 in vertex buffer array (len_vertices * floats per vertex)
  u32 len_vertex_buffer;
  u8 *vertex_buffer;

  u32 len_indices;
  u32 *indices;

  // blendshape data - deltas for this submesh's vertices
  u32 len_blendshapes;
  f32 *blendshape_deltas; // Interleaved: [pos_delta.xyz, normal_delta.xyz] per
                          // vertex per blendshape

  String material_path;
} SubMeshData;
arr_define(SubMeshData);

typedef struct {
  String mesh_name; // mesh name for blendshape animation mapping
  String32Bytes_Array blendshape_names; // shared across all submeshes
  SubMeshData_Array submeshes;
} MeshData;

// todo: rename Model3DData to Model3DAsset or something
typedef struct {
  u32 version;
  u32 num_meshes;
  MeshData *meshes;

  // joints SOA
  u32 len_joints;
  Joint *joints;
  String *joint_names;
} Model3DData;
slice_define(Model3DData);

typedef struct {
  u32 width;
  u32 height;
  u32 byte_len;
  u8 *data;
} Image;
slice_define(Image);
TYPED_HANDLE_DEFINE(Image);

typedef struct {
  Image image;
  Handle gpu_tex_handle;
} Texture;
TYPED_HANDLE_DEFINE(Texture);

typedef u16 RendererBatchId;
typedef struct {
  mat4 model_matrix;
} BatchInstanceData;

slice_define(BatchInstanceData);

#define MAX_DIRECTIONAL_LIGHTS 4
#define MAX_POINT_LIGHTS 4

// std140 layout
typedef struct packed_struct {
  // chunk 1
  vec3 direction;
  float _padding1;

  // chunk 2
  vec3 color;
  float intensity;
} DirectionalLight;

typedef struct packed_struct {
  // chunk 1
  float count;
  float _padding[3];

  DirectionalLight lights[MAX_DIRECTIONAL_LIGHTS];
} DirectionalLightBlock;

typedef struct packed_struct {
  // chunk 1
  vec3 position;
  float _padding1;

  // chunk 2
  vec3 color;
  float intensity;

  // chunk 3
  float innerRadius;
  float outerRadius;
  float _padding2[2];
} PointLight;

typedef struct packed_struct {
  float count;
  float _padding[3];
  PointLight lights[MAX_POINT_LIGHTS];
} PointLightsBlock;

typedef enum {
  SHADER_DEFINE_INVALID = 0,
  SHADER_DEFINE_BOOLEAN,
} ShaderDefineType;

typedef struct {
  String name;
  ShaderDefineType type;
  union {
    b32 flag;
  } value;
} ShaderDefine;
arr_define(ShaderDefine);

typedef struct {
  const char *shader_name; // e.g., "triangle", "pbr_lit", etc.
} LoadShaderParams;

#define SHADER_DEFINE_BOOL(define_name, _value)                                \
  {.name = STR_FROM_CSTR(define_name),                                         \
   .type = SHADER_DEFINE_BOOLEAN,                                              \
   .value.flag = _value}

// New dynamic material property system
typedef enum {
  MAT_PROP_INVALID = 0,
  MAT_PROP_TEXTURE,
  MAT_PROP_VEC3,
} MaterialPropertyType;

typedef struct {
  String name;
  MaterialPropertyType type;
  union {
    String texture_path;
    Color color;
  };
} MaterialAssetProperty;
arr_define(MaterialAssetProperty);

typedef struct {
  String name;
  String shader_path;
  b32 transparent;
  ShaderDefine_Array shader_defines;
  MaterialAssetProperty_Array properties;
} MaterialAsset;

typedef struct {
  String name;
  MaterialPropertyType type;
  union {
    Texture_Handle texture;
    vec3 vec3_val;
  } value;
} MaterialProperty;
arr_define(MaterialProperty);
slice_define(MaterialProperty);

typedef struct {
  MaterialAsset *asset;
  Handle gpu_material;
  MaterialProperty_Slice properties;
} Material;
arr_define(Material);
slice_define(Material);

// Convenience macros for creating material properties
#define MAT_PROP_TEX(_name, _tex_handle)                                       \
  {.name = STR_FROM_CSTR(_name),                                               \
   .type = MAT_PROP_TEXTURE,                                                   \
   .value.texture = _tex_handle}

#define MAT_PROP_VEC3(_name, x, y, z)                                          \
  {                                                                            \
    .name = str_from_cstr(_name), .type = MAT_PROP_VEC3, .value.vec3_val = {   \
      x,                                                                       \
      y,                                                                       \
      z                                                                        \
    }                                                                          \
  }

void renderer_init(gpu_device_t *device, Allocator *permanent_allocator, Allocator *temp_allocator);

void renderer_cleanup(void);

void renderer_execute_commands(gpu_texture_t *render_target, gpu_command_buffer_t *cmd_buffer);

void renderer_reset_commands(void);

void renderer_clear(Color color);

Handle load_shader(LoadShaderParams params);

// Internal helper to load shader with direct pipeline - used by video_renderer.c
Handle renderer_load_shader(const char *shader_name, gpu_pipeline_t *pipeline);

Handle renderer_create_submesh(SubMeshData *mesh_data, b32 is_skinned);

Handle load_material(Handle shader_handle, MaterialProperty *properties,
                     u32 property_count, b32 transparent);

void renderer_draw_mesh(Handle mesh_handle, Handle material_handle,
                        mat4 model_matrix);

void renderer_draw_skinned_mesh(Handle mesh_handle, Handle material_handle,
                                mat4 model_matrix, mat4 *joint_transforms,
                                u32 num_joints,
                                BlendshapeParams *blendshape_params);

Handle renderer_reserve_texture(void);

b32 renderer_set_texture(Handle tex_handle, Image *image);

void renderer_update_camera(const CameraUniformBlock *camera_uniforms);

void renderer_set_lights(const DirectionalLightBlock *lights);

void renderer_draw_skybox(Handle material_handle);

void renderer_handle_resize(i32 width, i32 height);

#endif