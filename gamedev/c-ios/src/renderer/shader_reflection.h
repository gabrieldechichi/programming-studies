#ifndef SHADER_REFLECTION_H
#define SHADER_REFLECTION_H

#include "lib/typedefs.h"
#include <string.h>

// Forward declarations
typedef struct gpu_pipeline gpu_pipeline_t;
typedef struct gpu_texture gpu_texture_t;

// Shader resource types
typedef enum {
    SHADER_RESOURCE_UNIFORM_BUFFER,
    SHADER_RESOURCE_STORAGE_BUFFER,
    SHADER_RESOURCE_TEXTURE,
    SHADER_RESOURCE_SAMPLER,
    SHADER_RESOURCE_PUSH_CONSTANT,
    SHADER_RESOURCE_IMAGE
} ShaderResourceType;

// Shader stages
typedef enum {
    SHADER_STAGE_VERTEX   = 1 << 0,
    SHADER_STAGE_FRAGMENT = 1 << 1,
    SHADER_STAGE_COMPUTE  = 1 << 2
} ShaderStageFlags;

// Data types for uniforms
typedef enum {
    UNIFORM_TYPE_FLOAT,
    UNIFORM_TYPE_VEC2,
    UNIFORM_TYPE_VEC3,
    UNIFORM_TYPE_VEC4,
    UNIFORM_TYPE_MAT3,
    UNIFORM_TYPE_MAT4,
    UNIFORM_TYPE_INT,
    UNIFORM_TYPE_IVEC2,
    UNIFORM_TYPE_IVEC3,
    UNIFORM_TYPE_IVEC4,
    UNIFORM_TYPE_UINT,
    UNIFORM_TYPE_BOOL
} UniformDataType;

// Texture dimensions
typedef enum {
    TEXTURE_DIM_1D = 1,
    TEXTURE_DIM_2D = 2,
    TEXTURE_DIM_3D = 3,
    TEXTURE_DIM_CUBE = 4
} TextureDimension;

// Describes a member of a uniform buffer
typedef struct UniformMember {
    const char* name;
    UniformDataType type;
    u32 offset;      // Byte offset in buffer
    u32 size;        // Size in bytes
    u32 array_count; // 1 for non-arrays
} UniformMember;

// Describes a single shader resource (uniform, texture, etc.)
typedef struct {
    const char* name;           // Resource name for lookup
    ShaderResourceType type;
    u32 binding;                // Binding point in shader
    u32 set;                    // Descriptor set (for Vulkan, usually 0)
    u32 size;                   // Size in bytes (for buffers)
    ShaderStageFlags stages;    // Which stages use this resource

    // Type-specific data
    union {
        struct { // For uniform buffers
            u32 member_count;
            const struct UniformMember* members;
        } uniform_buffer;

        struct { // For textures
            TextureDimension dimension;
            b32 is_array;
            b32 is_shadow;
        } texture;

        struct { // For storage buffers
            b32 readonly;
            b32 writeonly;
        } storage_buffer;
    } info;
} ShaderResourceDesc;

// Vertex attribute description
typedef struct {
    const char* name;
    u32 location;
    UniformDataType type;
    u32 offset;    // Offset in vertex buffer
    b32 normalized;
} VertexAttributeDesc;

// Semantic enums for fast lookups (no strings on hot path)
typedef enum {
    UNIFORM_SEMANTIC_CAMERA,
    UNIFORM_SEMANTIC_MODEL,
    UNIFORM_SEMANTIC_JOINTS,
    UNIFORM_SEMANTIC_MATERIAL,
    UNIFORM_SEMANTIC_LIGHTS,
    UNIFORM_SEMANTIC_BLENDSHAPES,
    UNIFORM_SEMANTIC_COUNT,
    UNIFORM_SEMANTIC_NONE = -1
} UniformSemantic;

typedef enum {
    TEXTURE_SEMANTIC_DIFFUSE,
    TEXTURE_SEMANTIC_NORMAL,
    TEXTURE_SEMANTIC_DETAIL,
    TEXTURE_SEMANTIC_ROUGHNESS,
    TEXTURE_SEMANTIC_METALLIC,
    TEXTURE_SEMANTIC_AO,
    TEXTURE_SEMANTIC_COUNT,
    TEXTURE_SEMANTIC_NONE = -1
} TextureSemantic;

// Maps resource names to semantic enums for fast runtime lookups
typedef struct {
    const char* resource_name;
    UniformSemantic uniform_semantic;
    TextureSemantic texture_semantic;
} SemanticMapping;

// Complete shader reflection data
typedef struct ShaderReflection {
    const char* name;

    // Shader file paths
    const char* vertex_shader_path;
    const char* fragment_shader_path;

    // Resources
    u32 resource_count;
    const ShaderResourceDesc* resources;

    // Vertex input (if applicable)
    u32 vertex_attribute_count;
    const VertexAttributeDesc* vertex_attributes;
    u32 vertex_stride;  // Total size of one vertex

    // Semantic mappings for fast lookups
    u32 semantic_mapping_count;
    const SemanticMapping* semantic_mappings;

    // Pipeline settings hints
    b32 depth_test;
    b32 depth_write;
    b32 alpha_blending;
    u32 cull_mode;  // 0 = none, 1 = back, 2 = front
} ShaderReflection;

// Material binding cache - computed once during material creation
typedef struct {
    u32 binding_index;          // Where to bind in pipeline
    ShaderResourceType type;
    union {
        struct {
            void* data;         // Pre-packed uniform data
            u32 size;
        } uniform;
        struct {
            u32 texture_handle_offset;  // Offset into material's texture array
        } texture;
    } resource;
} MaterialBinding;

typedef struct {
    MaterialBinding* bindings;
    u32 binding_count;
    void* uniform_data_block;   // Single allocation for all uniform data
    u32 uniform_data_size;
} MaterialBindingCache;

// Helper functions for working with reflection data
static inline const ShaderResourceDesc* find_resource_by_name(
    const ShaderReflection* reflection,
    const char* name
) {
    for (u32 i = 0; i < reflection->resource_count; i++) {
        if (strcmp(reflection->resources[i].name, name) == 0) {
            return &reflection->resources[i];
        }
    }
    return NULL;
}

static inline UniformSemantic get_uniform_semantic(
    const ShaderReflection* reflection,
    const char* resource_name
) {
    for (u32 i = 0; i < reflection->semantic_mapping_count; i++) {
        if (strcmp(reflection->semantic_mappings[i].resource_name, resource_name) == 0) {
            return reflection->semantic_mappings[i].uniform_semantic;
        }
    }
    return UNIFORM_SEMANTIC_NONE;
}

static inline TextureSemantic get_texture_semantic(
    const ShaderReflection* reflection,
    const char* resource_name
) {
    for (u32 i = 0; i < reflection->semantic_mapping_count; i++) {
        if (strcmp(reflection->semantic_mappings[i].resource_name, resource_name) == 0) {
            return reflection->semantic_mappings[i].texture_semantic;
        }
    }
    return TEXTURE_SEMANTIC_NONE;
}

// Size calculations for uniform types
static inline u32 get_uniform_type_size(UniformDataType type) {
    switch (type) {
        case UNIFORM_TYPE_FLOAT:    return 4;
        case UNIFORM_TYPE_VEC2:     return 8;
        case UNIFORM_TYPE_VEC3:     return 12;
        case UNIFORM_TYPE_VEC4:     return 16;
        case UNIFORM_TYPE_MAT3:     return 36;
        case UNIFORM_TYPE_MAT4:     return 64;
        case UNIFORM_TYPE_INT:      return 4;
        case UNIFORM_TYPE_IVEC2:    return 8;
        case UNIFORM_TYPE_IVEC3:    return 12;
        case UNIFORM_TYPE_IVEC4:    return 16;
        case UNIFORM_TYPE_UINT:     return 4;
        case UNIFORM_TYPE_BOOL:     return 4;
        default:                    return 0;
    }
}

#endif // SHADER_REFLECTION_H