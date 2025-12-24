#include "mesh.h"

MeshDesc mesh_asset_to_mesh(MeshBlobAsset *mesh_asset, Allocator *alloc) {
    f32 *positions = (f32 *)blob_array_get(mesh_asset, mesh_asset->positions);
    f32 *normals = (f32 *)blob_array_get(mesh_asset, mesh_asset->normals);

    u32 vertex_count = mesh_asset->vertex_count;
    u32 vertex_buffer_size = vertex_count * MESH_VERTEX_STRIDE;
    f32 *vertices = ALLOC_ARRAY(alloc, f32, vertex_count * 10);

    for (u32 i = 0; i < vertex_count; i++) {
        u32 dst = i * 10;
        u32 src3 = i * 3;

        vertices[dst + 0] = positions[src3 + 0];
        vertices[dst + 1] = positions[src3 + 1];
        vertices[dst + 2] = positions[src3 + 2];

        vertices[dst + 3] = normals[src3 + 0];
        vertices[dst + 4] = normals[src3 + 1];
        vertices[dst + 5] = normals[src3 + 2];

        vertices[dst + 6] = 1.0f;
        vertices[dst + 7] = 1.0f;
        vertices[dst + 8] = 1.0f;
        vertices[dst + 9] = 1.0f;
    }

    void *indices = blob_array_get(mesh_asset, mesh_asset->indices);
    GpuIndexFormat index_format = (mesh_asset->index_format == INDEX_FORMAT_U16)
                                      ? GPU_INDEX_FORMAT_U16
                                      : GPU_INDEX_FORMAT_U32;

    return (MeshDesc){
        .vertices = vertices,
        .vertex_size = vertex_buffer_size,
        .indices = indices,
        .index_size = mesh_asset->indices.size,
        .index_count = mesh_asset->index_count,
        .index_format = index_format,
    };
}
