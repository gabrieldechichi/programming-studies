#ifndef TOON_SHADING_REFLECTION_H
#define TOON_SHADING_REFLECTION_H

#include "../shader_reflection.h"
#include "renderer/renderer.h"

// Uniform buffer member descriptions
static const UniformMember toon_camera_members[] = {
    {"view_matrix",       UNIFORM_TYPE_MAT4, 0,   64, 1},
    {"projection_matrix", UNIFORM_TYPE_MAT4, 64,  64, 1},
    {"view_proj_matrix",  UNIFORM_TYPE_MAT4, 128, 64, 1},
    {"camera_pos",        UNIFORM_TYPE_VEC3, 192, 12, 1}
};

static const UniformMember toon_model_members[] = {
    {"model_matrix", UNIFORM_TYPE_MAT4, 0, 64, 1}
};

static const UniformMember toon_joint_members[] = {
    {"joint_matrices", UNIFORM_TYPE_MAT4, 0, 64, 256}  // Array of 256 matrices
};

static const UniformMember toon_material_members[] = {
    {"color", UNIFORM_TYPE_VEC3, 0, 12, 1}  // vec3, but will be padded to 16 bytes in shader
};

static const UniformMember toon_light_members[] = {
    {"light_count",    UNIFORM_TYPE_FLOAT, 0,  4,  1},
    {"padding",        UNIFORM_TYPE_VEC3,  4,  12, 1},
    {"light_dir_0",    UNIFORM_TYPE_VEC3,  16, 12, 1},
    {"padding_0",      UNIFORM_TYPE_FLOAT, 28, 4,  1},
    {"light_color_0",  UNIFORM_TYPE_VEC3,  32, 12, 1},
    {"light_intensity_0", UNIFORM_TYPE_FLOAT, 44, 4, 1}
    // Can add more lights here following the same pattern
};

static const UniformMember toon_blendshape_members[] = {
    {"weights",        UNIFORM_TYPE_FLOAT, 0,  4,  50}, // Array of 50 weights
    {"active_count",   UNIFORM_TYPE_INT,   200, 4, 1},
    {"vertex_id",      UNIFORM_TYPE_INT,   204, 4, 1}
};

// Resource descriptors for toon shading
static const ShaderResourceDesc toon_resources[] = {
    // Uniform buffers
    {
        .name = "camera_params",
        .type = SHADER_RESOURCE_UNIFORM_BUFFER,
        .binding = 0,
        .set = 0,
        .size = sizeof(CameraUniformBlock),
        .stages = SHADER_STAGE_VERTEX,
        .info.uniform_buffer = {
            .member_count = 4,
            .members = toon_camera_members
        }
    },
    {
        .name = "joint_transforms",
        .type = SHADER_RESOURCE_UNIFORM_BUFFER,
        .binding = 1,
        .set = 0,
        .size = sizeof(float) * 16 * 256,  // 256 mat4s
        .stages = SHADER_STAGE_VERTEX,
        .info.uniform_buffer = {
            .member_count = 1,
            .members = toon_joint_members
        }
    },
    {
        .name = "model_params",
        .type = SHADER_RESOURCE_UNIFORM_BUFFER,
        .binding = 2,
        .set = 0,
        .size = sizeof(float) * 16,  // 1 mat4
        .stages = SHADER_STAGE_VERTEX,
        .info.uniform_buffer = {
            .member_count = 1,
            .members = toon_model_members
        }
    },
    {
        .name = "material_params",
        .type = SHADER_RESOURCE_UNIFORM_BUFFER,
        .binding = 3,
        .set = 0,
        .size = 16,  // vec3 padded to 16 bytes
        .stages = SHADER_STAGE_FRAGMENT,
        .info.uniform_buffer = {
            .member_count = 1,
            .members = toon_material_members
        }
    },
    {
        .name = "light_params",
        .type = SHADER_RESOURCE_UNIFORM_BUFFER,
        .binding = 4,
        .set = 0,
        .size = sizeof(DirectionalLightBlock),
        .stages = SHADER_STAGE_FRAGMENT,
        .info.uniform_buffer = {
            .member_count = 8,  // count + padding + light data
            .members = toon_light_members
        }
    },
    {
        .name = "blendshape_params",
        .type = SHADER_RESOURCE_UNIFORM_BUFFER,
        .binding = 6,
        .set = 0,
        .size = sizeof(BlendshapeParams),
        .stages = SHADER_STAGE_VERTEX,
        .info.uniform_buffer = {
            .member_count = 3,
            .members = toon_blendshape_members
        }
    },

    // Textures
    {
        .name = "diffuse_texture",
        .type = SHADER_RESOURCE_TEXTURE,
        .binding = 5,
        .set = 0,
        .stages = SHADER_STAGE_FRAGMENT,
        .info.texture = {
            .dimension = TEXTURE_DIM_2D,
            .is_array = false,
            .is_shadow = false
        }
    },
    {
        .name = "detail_texture",
        .type = SHADER_RESOURCE_TEXTURE,
        .binding = 8,
        .set = 0,
        .stages = SHADER_STAGE_FRAGMENT,
        .info.texture = {
            .dimension = TEXTURE_DIM_2D,
            .is_array = false,
            .is_shadow = false
        }
    },

    // Storage buffer for blendshapes
    {
        .name = "blendshape_deltas",
        .type = SHADER_RESOURCE_STORAGE_BUFFER,
        .binding = 7,
        .set = 0,
        .size = sizeof(float) * 8 * 1000,  // Capacity for 1000 blendshape deltas
        .stages = SHADER_STAGE_VERTEX,
        .info.storage_buffer = {
            .readonly = true,
            .writeonly = false
        }
    }
};

// Vertex attributes for skinned mesh
static const VertexAttributeDesc toon_vertex_attributes[] = {
    {
        .name = "position",
        .location = 0,
        .type = UNIFORM_TYPE_VEC3,
        .offset = 0,
        .normalized = false
    },
    {
        .name = "normal",
        .location = 1,
        .type = UNIFORM_TYPE_VEC3,
        .offset = 12,
        .normalized = false
    },
    {
        .name = "uv",
        .location = 2,
        .type = UNIFORM_TYPE_VEC2,
        .offset = 24,
        .normalized = false
    },
    {
        .name = "joints",
        .location = 3,
        .type = UNIFORM_TYPE_IVEC4,  // Actually ubyte4, but treated as ivec4
        .offset = 32,
        .normalized = false
    },
    {
        .name = "weights",
        .location = 4,
        .type = UNIFORM_TYPE_VEC4,
        .offset = 36,
        .normalized = false
    }
};

// Semantic mappings for fast lookups
static const SemanticMapping toon_semantic_mappings[] = {
    {"camera_params",     UNIFORM_SEMANTIC_CAMERA,     TEXTURE_SEMANTIC_NONE},
    {"model_params",      UNIFORM_SEMANTIC_MODEL,      TEXTURE_SEMANTIC_NONE},
    {"joint_transforms",  UNIFORM_SEMANTIC_JOINTS,     TEXTURE_SEMANTIC_NONE},
    {"material_params",   UNIFORM_SEMANTIC_MATERIAL,   TEXTURE_SEMANTIC_NONE},
    {"light_params",      UNIFORM_SEMANTIC_LIGHTS,     TEXTURE_SEMANTIC_NONE},
    {"blendshape_params", UNIFORM_SEMANTIC_BLENDSHAPES, TEXTURE_SEMANTIC_NONE},
    {"diffuse_texture",   UNIFORM_SEMANTIC_NONE,       TEXTURE_SEMANTIC_DIFFUSE},
    {"detail_texture",    UNIFORM_SEMANTIC_NONE,       TEXTURE_SEMANTIC_DETAIL}
};

// Complete reflection data for toon shading
static const ShaderReflection toon_shading_reflection = {
    .name = "toon_shading",
    .vertex_shader_path = "toon_shading.vert.spv",
    .fragment_shader_path = "toon_shading.frag.spv",
    .resource_count = sizeof(toon_resources) / sizeof(toon_resources[0]),
    .resources = toon_resources,
    .vertex_attribute_count = sizeof(toon_vertex_attributes) / sizeof(toon_vertex_attributes[0]),
    .vertex_attributes = toon_vertex_attributes,
    .vertex_stride = 52,  // 3 + 3 + 2 + 1 + 4 floats = 13 * 4 = 52 bytes
    .semantic_mapping_count = sizeof(toon_semantic_mappings) / sizeof(toon_semantic_mappings[0]),
    .semantic_mappings = toon_semantic_mappings,
    .depth_test = true,
    .depth_write = true,
    .alpha_blending = false,
    .cull_mode = 1  // Back face culling
};

#endif // TOON_SHADING_REFLECTION_H