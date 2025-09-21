#ifndef RENDERER_H
#define RENDERER_H

#include "gpu_backend.h"
#include "lib/handle.h"
#include "lib/typedefs.h"
#include "vendor/cglm/types.h"

// ===== Public Type Definitions =====

// Material property types
typedef enum {
    MAT_PROP_INVALID = 0,
    MAT_PROP_VEC3,
    MAT_PROP_TEXTURE,
} MaterialPropertyType;

// Material property
typedef struct {
    String64Bytes name;
    MaterialPropertyType type;
    union {
        vec3 vec3_val;
        Handle texture;
    } value;
} MaterialProperty;

// Camera uniform block (matching toon shader expectations)
typedef struct {
    vec3 camera_pos;
    float _padding0;
    mat4 view_matrix;
    mat4 projection_matrix;
    mat4 view_proj_matrix;  // view * projection
} CameraUniformBlock;

// Directional light uniform block (matching toon shader)
typedef struct {
    float count;  // Number of lights
    float _pad0;
    float _pad1;
    float _pad2;
    vec4 lights_data[8];  // 4 lights * 2 vec4s each (direction + color/intensity)
} DirectionalLightBlock;

// Blendshape parameters
typedef struct {
    float weights[32]; // Support up to 32 blendshapes
} BlendshapeParams;

// Mesh data for creation
typedef struct {
    float *vertex_buffer;
    u32 *indices;
    u32 len_vertex_buffer;
    u32 len_indices;
    u32 len_vertices;
    u32 len_blendshapes;
    float *blendshape_deltas;
} SubMeshData;

// Color type
typedef struct {
    union {
        struct {
            float r, g, b, a;
        };
        vec4 components;
    };
} Color;

// ===== Public API =====

// Initialize the renderer
void renderer_init(gpu_device_t *device, Allocator *permanent_allocator, Allocator *temp_allocator);

// Cleanup the renderer
void renderer_cleanup(void);

// Reset render commands for new frame
void renderer_reset_commands(void);

// Clear command
void renderer_clear(Color color);

// Create a submesh
Handle renderer_create_submesh(SubMeshData *mesh_data, b32 is_skinned);

// Load shader (takes an already created pipeline)
Handle renderer_load_shader(const char *shader_name, gpu_pipeline_t *pipeline);

// Load material
Handle renderer_load_material(Handle shader_handle, MaterialProperty *properties,
                              u32 property_count, b32 transparent);

// Draw simple mesh (no skinning)
void renderer_draw_mesh(Handle mesh_handle, Handle material_handle,
                       mat4 model_matrix);

// Draw skinned mesh
void renderer_draw_skinned_mesh(Handle mesh_handle, Handle material_handle,
                                mat4 model_matrix, mat4 *joint_transforms,
                                u32 num_joints, BlendshapeParams *blendshape_params);

// Update camera uniforms
void renderer_update_camera(const CameraUniformBlock *camera_uniforms);

// Set lighting
void renderer_set_lights(const DirectionalLightBlock *lights);

// Execute accumulated render commands
void renderer_execute_commands(gpu_texture_t *render_target, gpu_command_buffer_t *cmd_buffer);

// Helper function to create a cube mesh
SubMeshData* create_cube_mesh_data(Allocator *allocator);

#endif // RENDERER_H