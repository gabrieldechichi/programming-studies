#ifndef H_MESH
#define H_MESH

#include "blob_asset.h"

#define MESH_VERTEX_STRIDE 48

typedef enum {
  INDEX_FORMAT_U16 = 0,
  INDEX_FORMAT_U32 = 1,
} IndexFormat;

typedef struct {
  StringBlob name;
  IndexFormat index_format;
  u32 index_count;
  u32 vertex_count;
  BlobArray(void) indices; // can be u32 or u16
  BlobArray(f32) positions;
  BlobArray(f32) normals;
  BlobArray(f32) tangents;
  BlobArray(f32) uvs;
} MeshBlobAsset;

typedef struct {
  BlobAssetHeader header;
  u32 mesh_count;
  BlobArray(MeshBlobAsset) meshes;
} ModelBlobAsset;

#ifndef MESH_TYPES_ONLY
#include "renderer.h"
MeshDesc mesh_asset_to_mesh(MeshBlobAsset *mesh_asset, Allocator *alloc);
#endif

#define STATIC_MESH_VERTEX_LAYOUT                                              \
  (GpuVertexLayout) {                                                          \
    .stride = MESH_VERTEX_STRIDE,                                              \
    .attrs = FIXED_ARRAY_DEFINE(GpuVertexAttr,                                 \
                                {.format = GPU_VERTEX_FORMAT_FLOAT3,           \
                                 .offset = 0,                                  \
                                 .shader_location = 0},                        \
                                {.format = GPU_VERTEX_FORMAT_FLOAT3,           \
                                 .offset = 12,                                 \
                                 .shader_location = 1},                        \
                                {.format = GPU_VERTEX_FORMAT_FLOAT4,           \
                                 .offset = 24,                                 \
                                 .shader_location = 2},                        \
                                {.format = GPU_VERTEX_FORMAT_FLOAT2,           \
                                 .offset = 40,                                 \
                                 .shader_location = 3}, ),                     \
  }

#endif
