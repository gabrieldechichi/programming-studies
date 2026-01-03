#include "context.h"
#include "lib/thread_context.h"
#include "lib/typedefs.h"
#include "os/os.h"
#include "app.h"
#include "mesh.h"

typedef enum {
    LOAD_STATE_IDLE,
    LOAD_STATE_LOADING,
    LOAD_STATE_DONE,
    LOAD_STATE_ERROR,
} LoadState;

typedef struct {
    LoadState load_state;
    OsFileOp *file_op;
    u8 *asset_data;
    u32 asset_size;
} GameState;

global GameState g_state;

void app_init(AppMemory *memory) {
    UNUSED(memory);
    if (!is_main_thread()) {
        return;
    }

    g_state.load_state = LOAD_STATE_IDLE;
    g_state.file_op = NULL;
    g_state.asset_data = NULL;
    g_state.asset_size = 0;

    LOG_INFO("Asset loading demo initialized");
    LOG_INFO("Starting to load cube.hasset...");

    g_state.file_op = os_start_read_file("cube.hasset");
    if (g_state.file_op) {
        g_state.load_state = LOAD_STATE_LOADING;
    } else {
        LOG_ERROR("Failed to start file read");
        g_state.load_state = LOAD_STATE_ERROR;
    }
}

void log_mesh_data(ModelBlobAsset *model, u8 *blob) {
    LOG_INFO("=== ModelBlobAsset ===");
    LOG_INFO("  Version: %", FMT_UINT(model->header.version));
    LOG_INFO("  Asset size: % bytes", FMT_UINT(model->header.asset_size));
    LOG_INFO("  Mesh count: %", FMT_UINT(model->mesh_count));

    MeshBlobAsset *meshes = (MeshBlobAsset *)(blob + model->meshes.offset);

    for (u32 i = 0; i < model->mesh_count; i++) {
        MeshBlobAsset *mesh = &meshes[i];
        char *name = string_blob_get(mesh, mesh->name);

        LOG_INFO("--- Mesh % ---", FMT_UINT(i));
        LOG_INFO("  Name: %", FMT_STR(name));
        LOG_INFO("  Index format: %", FMT_STR(mesh->index_format == INDEX_FORMAT_U16 ? "u16" : "u32"));
        LOG_INFO("  Index count: %", FMT_UINT(mesh->index_count));
        LOG_INFO("  Vertex count: %", FMT_UINT(mesh->vertex_count));

        f32 *positions = (f32 *)blob_array_get(f32, mesh, mesh->positions);
        f32 *normals = (f32 *)blob_array_get(f32, mesh, mesh->normals);

        LOG_INFO("  First position: (%, %, %)",
                 FMT_FLOAT(positions[0]),
                 FMT_FLOAT(positions[1]),
                 FMT_FLOAT(positions[2]));

        LOG_INFO("  First normal: (%, %, %)",
                 FMT_FLOAT(normals[0]),
                 FMT_FLOAT(normals[1]),
                 FMT_FLOAT(normals[2]));

        if (mesh->index_format == INDEX_FORMAT_U16) {
            u16 *indices = (u16 *)blob_array_get(u16, mesh, mesh->indices);
            LOG_INFO("  First triangle: %, %, %",
                     FMT_UINT(indices[0]),
                     FMT_UINT(indices[1]),
                     FMT_UINT(indices[2]));
        } else {
            u32 *indices = (u32 *)blob_array_get(f32, mesh, mesh->indices);
            LOG_INFO("  First triangle: %, %, %",
                     FMT_UINT(indices[0]),
                     FMT_UINT(indices[1]),
                     FMT_UINT(indices[2]));
        }
    }
}

void app_update_and_render(AppMemory *memory) {
    UNUSED(memory);

    if (!is_main_thread()) {
        return;
    }

    if (g_state.load_state == LOAD_STATE_LOADING) {
        OsFileReadState read_state = os_check_read_file(g_state.file_op);

        if (read_state == OS_FILE_READ_STATE_COMPLETED) {
            AppContext *app_ctx = app_ctx_current();
            Allocator alloc = make_arena_allocator(&app_ctx->arena);

            PlatformFileData file_data = {0};
            if (os_get_file_data(g_state.file_op, &file_data, &alloc)) {
                g_state.asset_data = file_data.buffer;
                g_state.asset_size = file_data.buffer_len;
                g_state.load_state = LOAD_STATE_DONE;

                LOG_INFO("File loaded: % bytes", FMT_UINT(g_state.asset_size));

                ModelBlobAsset *model = (ModelBlobAsset *)g_state.asset_data;
                log_mesh_data(model, g_state.asset_data);
            } else {
                LOG_ERROR("Failed to get file data");
                g_state.load_state = LOAD_STATE_ERROR;
            }
        } else if (read_state == OS_FILE_READ_STATE_ERROR) {
            LOG_ERROR("File read error");
            g_state.load_state = LOAD_STATE_ERROR;
        }
    }
}
