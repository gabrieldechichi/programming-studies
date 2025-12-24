#ifndef H_MESH
#define H_MESH

#include "blob_asset.h"

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

#endif
