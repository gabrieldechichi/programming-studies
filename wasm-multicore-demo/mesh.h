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
    BlobArray indices;
    BlobArray positions;
    BlobArray normals;
    BlobArray tangents;
    BlobArray uvs;
} MeshBlobAsset;

typedef struct {
    BlobAssetHeader header;
    u32 mesh_count;
    BlobArray meshes;
} ModelBlobAsset;

#ifndef MESH_TYPES_ONLY
#include "renderer.h"
MeshDesc mesh_asset_to_mesh(MeshBlobAsset *mesh_asset, Allocator *alloc);
#endif

#endif
