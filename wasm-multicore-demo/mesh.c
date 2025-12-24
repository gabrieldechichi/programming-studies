#include "mesh.h"

MeshDesc mesh_asset_to_mesh(MeshBlobAsset *mesh_asset, Allocator *alloc) {
    f32 *positions = (f32 *)blob_array_get(mesh_asset, mesh_asset->positions);
    f32 *normals = (f32 *)blob_array_get(mesh_asset, mesh_asset->normals);
    f32 *tangents = (f32 *)blob_array_get(mesh_asset, mesh_asset->tangents);
    f32 *uvs = (f32 *)blob_array_get(mesh_asset, mesh_asset->uvs);

    u32 vertex_count = mesh_asset->vertex_count;
    u32 vertex_buffer_size = vertex_count * MESH_VERTEX_STRIDE;
    u32 floats_per_vertex = MESH_VERTEX_STRIDE / sizeof(f32);
    f32 *vertices = ALLOC_ARRAY(alloc, f32, vertex_count * floats_per_vertex);

    for (u32 i = 0; i < vertex_count; i++) {
        u32 dst = i * floats_per_vertex;
        u32 src3 = i * 3;
        u32 src4 = i * 4;
        u32 src2 = i * 2;

        vertices[dst + 0] = positions[src3 + 0];
        vertices[dst + 1] = positions[src3 + 1];
        vertices[dst + 2] = positions[src3 + 2];

        vertices[dst + 3] = normals[src3 + 0];
        vertices[dst + 4] = normals[src3 + 1];
        vertices[dst + 5] = normals[src3 + 2];

        vertices[dst + 6] = tangents[src4 + 0];
        vertices[dst + 7] = tangents[src4 + 1];
        vertices[dst + 8] = tangents[src4 + 2];
        vertices[dst + 9] = tangents[src4 + 3];

        vertices[dst + 10] = uvs[src2 + 0];
        vertices[dst + 11] = uvs[src2 + 1];
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
