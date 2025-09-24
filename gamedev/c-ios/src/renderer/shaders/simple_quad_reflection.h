#ifndef SIMPLE_QUAD_REFLECTION_H
#define SIMPLE_QUAD_REFLECTION_H

#include "../shader_reflection.h"
#include "renderer/renderer.h"

// Uniform buffer member descriptions
static const UniformMember simple_quad_camera_members[] = {
    {"view",         UNIFORM_TYPE_MAT4, 0,   64, 1},
    {"projection",   UNIFORM_TYPE_MAT4, 64,  64, 1},
    {"viewProj",     UNIFORM_TYPE_MAT4, 128, 64, 1},
    {"cameraPos",    UNIFORM_TYPE_VEC3, 192, 12, 1}
};

static const UniformMember simple_quad_model_members[] = {
    {"model", UNIFORM_TYPE_MAT4, 0, 64, 1}
};

// Resource descriptors for simple quad shader
static const ShaderResourceDesc simple_quad_resources[] = {
    // Camera uniform buffer
    {
        .name = "camera_params",
        .type = SHADER_RESOURCE_UNIFORM_BUFFER,
        .binding = 0,
        .set = 0,
        .size = sizeof(CameraUniformBlock),
        .stages = SHADER_STAGE_VERTEX,
        .info.uniform_buffer = {
            .member_count = 4,
            .members = simple_quad_camera_members
        }
    },
    // Model uniform buffer
    {
        .name = "model_params",
        .type = SHADER_RESOURCE_UNIFORM_BUFFER,
        .binding = 1,
        .set = 0,
        .size = sizeof(float) * 16,  // 1 mat4
        .stages = SHADER_STAGE_VERTEX,
        .info.uniform_buffer = {
            .member_count = 1,
            .members = simple_quad_model_members
        }
    },
    // Diffuse texture
    {
        .name = "diffuse_texture",
        .type = SHADER_RESOURCE_TEXTURE,
        .binding = 2,
        .set = 0,
        .stages = SHADER_STAGE_FRAGMENT,
        .info.texture = {
            .dimension = TEXTURE_DIM_2D,
            .is_array = false,
            .is_shadow = false
        }
    }
};

// Vertex attributes for simple quad
static const VertexAttributeDesc simple_quad_vertex_attributes[] = {
    {
        .name = "position",
        .location = 0,
        .type = UNIFORM_TYPE_VEC3,
        .offset = 0,
        .normalized = false
    },
    {
        .name = "texcoord",
        .location = 1,
        .type = UNIFORM_TYPE_VEC2,
        .offset = 12,
        .normalized = false
    }
};

// Semantic mappings for fast lookups
static const SemanticMapping simple_quad_semantic_mappings[] = {
    {"camera_params",  UNIFORM_SEMANTIC_CAMERA, TEXTURE_SEMANTIC_NONE},
    {"model_params",   UNIFORM_SEMANTIC_MODEL,  TEXTURE_SEMANTIC_NONE},
    {"diffuse_texture", UNIFORM_SEMANTIC_NONE,  TEXTURE_SEMANTIC_DIFFUSE}
};

// Complete reflection data for simple quad shader
static const ShaderReflection simple_quad_reflection = {
    .name = "simple_quad",
    .vertex_shader_path = "simple_quad.vert.spv",
    .fragment_shader_path = "simple_quad.frag.spv",
    .resource_count = sizeof(simple_quad_resources) / sizeof(simple_quad_resources[0]),
    .resources = simple_quad_resources,
    .vertex_attribute_count = sizeof(simple_quad_vertex_attributes) / sizeof(simple_quad_vertex_attributes[0]),
    .vertex_attributes = simple_quad_vertex_attributes,
    .vertex_stride = 20,  // 3 floats (position) + 2 floats (uv) = 5 * 4 = 20 bytes
    .semantic_mapping_count = sizeof(simple_quad_semantic_mappings) / sizeof(simple_quad_semantic_mappings[0]),
    .semantic_mappings = simple_quad_semantic_mappings,
    .depth_test = true,
    .depth_write = true,
    .alpha_blending = false,
    .cull_mode = 0  // No culling for quad (to see it from both sides)
};

#endif // SIMPLE_QUAD_REFLECTION_H