#include "lib/typedefs.h"
#include "lib/memory.h"
#include "lib/string.h"
#include "lib/string_builder.h"
#include "lib/array.h"
#include "lib/hash.h"
#include "lib/thread_context.h"
#include "lib/multicore_runtime.h"
#include "os/os.h"
#define MESH_TYPES_ONLY
#include "mesh.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include "lib/string.c"
#include "lib/common.c"
#include "lib/memory.c"
#include "lib/allocator_pool.c"
#include "lib/string_builder.c"
#include "lib/cmd_line.c"
#include "lib/thread.c"
#include "lib/thread_context.c"
#include "lib/task.c"
#include "lib/multicore_runtime.c"
#include "os/os_win32.c"

arr_define(u16);

typedef struct {
    String name;
    IndexFormat index_format;
    u32 index_count;
    u32 vertex_count;
    u8 *indices;
    u32 indices_size;
    f32 *positions;
    f32 *normals;
    f32 *tangents;
    f32 *uvs;
} TempMeshData;

arr_define(TempMeshData);

static void *cgltf_alloc_func(void *user, cgltf_size size) {
    Allocator *alloc = (Allocator *)user;
    return ALLOC_ARRAY(alloc, u8, size);
}

static void cgltf_free_func(void *user, void *ptr) {
    UNUSED(user);
    UNUSED(ptr);
}

local_shared i32 g_argc;
local_shared char **g_argv;

void print_usage(void) {
    LOG_INFO("Usage: exporter --input <path.glb> --output <path.hasset>");
    LOG_INFO("Options:");
    LOG_INFO("  --input   Path to input .glb file");
    LOG_INFO("  --output  Path to output .hasset file");
}

void entrypoint(void) {
    local_shared Allocator allocator;
    local_shared ArenaAllocator arena;
    local_shared b32 parse_success;
    local_shared String input_path;
    local_shared String output_path;

    if (is_main_thread()) {
        os_time_init();

        const u64 arena_size = MB(256);
        void *memory = os_allocate_memory(arena_size);
        arena = arena_from_buffer(memory, arena_size);
        allocator = make_arena_allocator(&arena);

        CmdLineParser parser = cmdline_create(&allocator);

        cmdline_add_option(&parser, "input");
        cmdline_add_option(&parser, "output");

        parse_success = cmdline_parse(&parser, g_argc, g_argv);

        if (parse_success) {
            input_path = cmdline_get_option(&parser, "input");
            output_path = cmdline_get_option(&parser, "output");

            if (input_path.len == 0 || output_path.len == 0) {
                LOG_ERROR("Missing required options --input and --output");
                print_usage();
                parse_success = false;
            }
        }

        if (!parse_success) {
            print_usage();
        }
    }
    lane_sync();

    if (!parse_success) {
        return;
    }

    if (is_main_thread()) {
        LOG_INFO("Exporter started");
        LOG_INFO("  Input:  %", FMT_STR_VIEW(input_path));
        LOG_INFO("  Output: %", FMT_STR_VIEW(output_path));
    }
    lane_sync();

    if (!is_main_thread()) {
        return;
    }

    ThreadContext *tctx = tctx_current();
    Allocator temp_alloc = make_arena_allocator(&tctx->temp_arena);
    Allocator *temp = &temp_alloc;

    char input_cstr[512];
    memcpy(input_cstr, input_path.value, input_path.len);
    input_cstr[input_path.len] = '\0';

    cgltf_options options = {0};
    options.memory.alloc_func = cgltf_alloc_func;
    options.memory.free_func = cgltf_free_func;
    options.memory.user_data = temp;

    cgltf_data *gltf = NULL;
    cgltf_result result = cgltf_parse_file(&options, input_cstr, &gltf);
    if (result != cgltf_result_success) {
        LOG_ERROR("Failed to parse glTF file: %", FMT_STR_VIEW(input_path));
        return;
    }

    result = cgltf_load_buffers(&options, gltf, input_cstr);
    if (result != cgltf_result_success) {
        LOG_ERROR("Failed to load glTF buffers");
        return;
    }

    LOG_INFO("Parsed glTF: % meshes", FMT_UINT(gltf->meshes_count));

    u32 total_primitives = 0;
    for (u32 i = 0; i < gltf->meshes_count; i++) {
        total_primitives += (u32)gltf->meshes[i].primitives_count;
    }
    LOG_INFO("Total primitives: %", FMT_UINT(total_primitives));

    TempMeshData_DynArray temp_meshes = dyn_arr_new_alloc(temp, TempMeshData, total_primitives);

    for (u32 mesh_idx = 0; mesh_idx < gltf->meshes_count; mesh_idx++) {
        cgltf_mesh *mesh = &gltf->meshes[mesh_idx];

        for (u32 prim_idx = 0; prim_idx < mesh->primitives_count; prim_idx++) {
            cgltf_primitive *prim = &mesh->primitives[prim_idx];

            if (prim->type != cgltf_primitive_type_triangles) {
                LOG_WARN("Skipping non-triangle primitive");
                continue;
            }

            TempMeshData temp_mesh = {0};

            char name_buf[128];
            StringBuilder sb;
            sb_init(&sb, name_buf, sizeof(name_buf));

            if (mesh->name) {
                if (mesh->primitives_count > 1) {
                    sb_append_format(&sb, "%_p%", FMT_STR(mesh->name), FMT_UINT(prim_idx));
                } else {
                    sb_append(&sb, mesh->name);
                }
            } else {
                sb_append_format(&sb, "mesh_%_p%", FMT_UINT(mesh_idx), FMT_UINT(prim_idx));
            }

            u32 name_len = (u32)sb_length(&sb);
            char *name_copy = ALLOC_ARRAY(temp, char, name_len + 1);
            memcpy(name_copy, name_buf, name_len + 1);
            temp_mesh.name = (String){.value = name_copy, .len = name_len};

            cgltf_accessor *pos_accessor = NULL;
            cgltf_accessor *norm_accessor = NULL;
            cgltf_accessor *tan_accessor = NULL;
            cgltf_accessor *uv_accessor = NULL;

            for (u32 attr_idx = 0; attr_idx < prim->attributes_count; attr_idx++) {
                cgltf_attribute *attr = &prim->attributes[attr_idx];
                if (attr->type == cgltf_attribute_type_position) {
                    pos_accessor = attr->data;
                } else if (attr->type == cgltf_attribute_type_normal) {
                    norm_accessor = attr->data;
                } else if (attr->type == cgltf_attribute_type_tangent) {
                    tan_accessor = attr->data;
                } else if (attr->type == cgltf_attribute_type_texcoord && attr->index == 0) {
                    uv_accessor = attr->data;
                }
            }

            if (!pos_accessor) {
                LOG_ERROR("Primitive missing position attribute");
                continue;
            }

            temp_mesh.vertex_count = (u32)pos_accessor->count;

            temp_mesh.positions = ALLOC_ARRAY(temp, f32, temp_mesh.vertex_count * 3);
            cgltf_accessor_unpack_floats(pos_accessor, temp_mesh.positions, temp_mesh.vertex_count * 3);

            if (norm_accessor) {
                temp_mesh.normals = ALLOC_ARRAY(temp, f32, temp_mesh.vertex_count * 3);
                cgltf_accessor_unpack_floats(norm_accessor, temp_mesh.normals, temp_mesh.vertex_count * 3);
            } else {
                temp_mesh.normals = ALLOC_ARRAY(temp, f32, temp_mesh.vertex_count * 3);
                for (u32 i = 0; i < temp_mesh.vertex_count * 3; i++) {
                    temp_mesh.normals[i] = 0.0f;
                }
            }

            if (tan_accessor) {
                temp_mesh.tangents = ALLOC_ARRAY(temp, f32, temp_mesh.vertex_count * 4);
                cgltf_accessor_unpack_floats(tan_accessor, temp_mesh.tangents, temp_mesh.vertex_count * 4);
            } else {
                temp_mesh.tangents = ALLOC_ARRAY(temp, f32, temp_mesh.vertex_count * 4);
                for (u32 i = 0; i < temp_mesh.vertex_count * 4; i++) {
                    temp_mesh.tangents[i] = 0.0f;
                }
            }

            if (uv_accessor) {
                temp_mesh.uvs = ALLOC_ARRAY(temp, f32, temp_mesh.vertex_count * 2);
                cgltf_accessor_unpack_floats(uv_accessor, temp_mesh.uvs, temp_mesh.vertex_count * 2);
            } else {
                temp_mesh.uvs = ALLOC_ARRAY(temp, f32, temp_mesh.vertex_count * 2);
                for (u32 i = 0; i < temp_mesh.vertex_count * 2; i++) {
                    temp_mesh.uvs[i] = 0.0f;
                }
            }

            if (prim->indices) {
                temp_mesh.index_count = (u32)prim->indices->count;

                if (temp_mesh.vertex_count <= 65535) {
                    temp_mesh.index_format = INDEX_FORMAT_U16;
                    temp_mesh.indices_size = temp_mesh.index_count * sizeof(u16);
                    temp_mesh.indices = ALLOC_ARRAY(temp, u8, temp_mesh.indices_size);
                    u16 *indices_u16 = (u16 *)temp_mesh.indices;
                    for (u32 i = 0; i < temp_mesh.index_count; i++) {
                        indices_u16[i] = (u16)cgltf_accessor_read_index(prim->indices, i);
                    }
                } else {
                    temp_mesh.index_format = INDEX_FORMAT_U32;
                    temp_mesh.indices_size = temp_mesh.index_count * sizeof(u32);
                    temp_mesh.indices = ALLOC_ARRAY(temp, u8, temp_mesh.indices_size);
                    u32 *indices_u32 = (u32 *)temp_mesh.indices;
                    for (u32 i = 0; i < temp_mesh.index_count; i++) {
                        indices_u32[i] = (u32)cgltf_accessor_read_index(prim->indices, i);
                    }
                }
            } else {
                temp_mesh.index_count = temp_mesh.vertex_count;
                temp_mesh.index_format = INDEX_FORMAT_U16;
                temp_mesh.indices_size = temp_mesh.index_count * sizeof(u16);
                temp_mesh.indices = ALLOC_ARRAY(temp, u8, temp_mesh.indices_size);
                u16 *indices_u16 = (u16 *)temp_mesh.indices;
                for (u32 i = 0; i < temp_mesh.index_count; i++) {
                    indices_u16[i] = (u16)i;
                }
            }

            arr_append(temp_meshes, temp_mesh);
            LOG_INFO("  Mesh '%': % verts, % indices",
                     FMT_STR_VIEW(temp_mesh.name),
                     FMT_UINT(temp_mesh.vertex_count),
                     FMT_UINT(temp_mesh.index_count));
        }
    }

    u32 mesh_count = temp_meshes.len;

    u64 total_size = sizeof(ModelBlobAsset);
    total_size += mesh_count * sizeof(MeshBlobAsset);

    u64 strings_offset = total_size;
    for (u32 i = 0; i < mesh_count; i++) {
        total_size += temp_meshes.items[i].name.len + 1;
    }

    total_size = (total_size + 15) & ~15;
    u64 data_offset = total_size;

    for (u32 i = 0; i < mesh_count; i++) {
        TempMeshData *m = &temp_meshes.items[i];
        total_size += m->indices_size;
        total_size += m->vertex_count * 3 * sizeof(f32);
        total_size += m->vertex_count * 3 * sizeof(f32);
        total_size += m->vertex_count * 4 * sizeof(f32);
        total_size += m->vertex_count * 2 * sizeof(f32);
    }

    LOG_INFO("Total blob size: % bytes", FMT_UINT(total_size));

    u8 *blob = ALLOC_ARRAY(temp, u8, total_size);
    memset(blob, 0, total_size);

    ModelBlobAsset *model = (ModelBlobAsset *)blob;
    model->header.version = ASSET_VERSION;
    model->header.asset_size = total_size;
    model->header.asset_type_hash = fnv1a_hash("ModelBlobAsset");
    model->header.dependency_count = 0;
    model->mesh_count = mesh_count;

    u64 meshes_offset = sizeof(ModelBlobAsset);
    model->meshes.offset = (u32)meshes_offset;
    model->meshes.size = mesh_count * sizeof(MeshBlobAsset);
    model->meshes.type_size = sizeof(MeshBlobAsset);
    model->meshes.typehash = TYPE_HASH(MeshBlobAsset);

    MeshBlobAsset *mesh_array = (MeshBlobAsset *)(blob + meshes_offset);

    u64 current_string_offset = strings_offset;
    u64 current_data_offset = data_offset;

    for (u32 i = 0; i < mesh_count; i++) {
        TempMeshData *src = &temp_meshes.items[i];
        MeshBlobAsset *dst = &mesh_array[i];

        dst->name.len = src->name.len;
        dst->name.offset = (u32)(current_string_offset - (meshes_offset + i * sizeof(MeshBlobAsset)));
        memcpy(blob + current_string_offset, src->name.value, src->name.len + 1);
        current_string_offset += src->name.len + 1;

        dst->index_format = src->index_format;
        dst->index_count = src->index_count;
        dst->vertex_count = src->vertex_count;

        u64 mesh_base = meshes_offset + i * sizeof(MeshBlobAsset);

        dst->indices.offset = (u32)(current_data_offset - mesh_base);
        dst->indices.size = src->indices_size;
        dst->indices.type_size = (src->index_format == INDEX_FORMAT_U16) ? sizeof(u16) : sizeof(u32);
        dst->indices.typehash = (src->index_format == INDEX_FORMAT_U16) ? TYPE_HASH(u16) : TYPE_HASH(u32);
        memcpy(blob + current_data_offset, src->indices, src->indices_size);
        current_data_offset += src->indices_size;

        u32 pos_size = src->vertex_count * 3 * sizeof(f32);
        dst->positions.offset = (u32)(current_data_offset - mesh_base);
        dst->positions.size = pos_size;
        dst->positions.type_size = sizeof(f32);
        dst->positions.typehash = TYPE_HASH(f32);
        memcpy(blob + current_data_offset, src->positions, pos_size);
        current_data_offset += pos_size;

        u32 norm_size = src->vertex_count * 3 * sizeof(f32);
        dst->normals.offset = (u32)(current_data_offset - mesh_base);
        dst->normals.size = norm_size;
        dst->normals.type_size = sizeof(f32);
        dst->normals.typehash = TYPE_HASH(f32);
        memcpy(blob + current_data_offset, src->normals, norm_size);
        current_data_offset += norm_size;

        u32 tan_size = src->vertex_count * 4 * sizeof(f32);
        dst->tangents.offset = (u32)(current_data_offset - mesh_base);
        dst->tangents.size = tan_size;
        dst->tangents.type_size = sizeof(f32);
        dst->tangents.typehash = TYPE_HASH(f32);
        memcpy(blob + current_data_offset, src->tangents, tan_size);
        current_data_offset += tan_size;

        u32 uv_size = src->vertex_count * 2 * sizeof(f32);
        dst->uvs.offset = (u32)(current_data_offset - mesh_base);
        dst->uvs.size = uv_size;
        dst->uvs.type_size = sizeof(f32);
        dst->uvs.typehash = TYPE_HASH(f32);
        memcpy(blob + current_data_offset, src->uvs, uv_size);
        current_data_offset += uv_size;
    }

    char output_cstr[512];
    memcpy(output_cstr, output_path.value, output_path.len);
    output_cstr[output_path.len] = '\0';

    b32 write_success = os_write_file(output_cstr, blob, total_size);
    if (write_success) {
        LOG_INFO("Export complete: %", FMT_STR_VIEW(output_path));
    } else {
        LOG_ERROR("Failed to write output file: %", FMT_STR_VIEW(output_path));
    }
}

int main(int argc, char *argv[]) {
    g_argc = argc;
    g_argv = argv;

    os_init();
    i32 num_cores = os_get_processor_count();
    i32 thread_count = MAX(1, num_cores);

    const u64 runtime_arena_size = MB(64);
    void *runtime_memory = os_allocate_memory(runtime_arena_size);
    ArenaAllocator runtime_arena = arena_from_buffer(runtime_memory, runtime_arena_size);

    mcr_run((u8)thread_count, MB(4), entrypoint, &runtime_arena);

    return 0;
}
