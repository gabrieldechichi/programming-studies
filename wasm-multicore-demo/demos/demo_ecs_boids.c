#include "context.h"
#include "lib/thread_context.h"
#include "lib/typedefs.h"
#include "os/os.h"
#include "lib/math.h"
#include "lib/hash.h"
#include "cube.h"
#include "renderer.h"
#include "camera.h"
#include "app.h"

#include "ecs/ecs_entity.c"
#include "ecs/ecs_table.c"

#define VERTEX_STRIDE 40
#define VERTEX_NORMAL_OFFSET 12
#define VERTEX_COLOR_OFFSET 24

#define NUM_BOIDS 10000
#define NUM_TARGETS 2
#define NUM_OBSTACLES 1

#define GRID_SIZE (8192)
#define MAX_PER_BUCKET 64
#define CELL_SIZE 8.0f

#define BOID_SEPARATION_WEIGHT 1.0f
#define BOID_ALIGNMENT_WEIGHT 1.0f
#define BOID_TARGET_WEIGHT 2.0f
#define BOID_OBSTACLE_AVERSION_DISTANCE 30.0f
#define BOID_MOVE_SPEED 25.0f

typedef struct { f32 x, y, z; } Position;
typedef struct { f32 x, y, z; } Heading;
typedef struct { u8 dummy; } BoidTag;
typedef struct { u8 dummy; } TargetTag;
typedef struct { u8 dummy; } ObstacleTag;

typedef struct {
    f32 px, py, pz;
    f32 hx, hy, hz;
    u32 boid_idx;
} BoidBucketEntry;

typedef struct {
    u32 count;
    BoidBucketEntry entries[MAX_PER_BUCKET];
} BoidBucket;

global BoidBucket g_buckets[GRID_SIZE];

global EcsWorld g_world;
global Camera g_camera;

global GpuMesh_Handle g_cube_mesh;
global Material_Handle g_cube_material;
global InstanceBuffer_Handle g_instance_buffer;
global mat4 g_instance_data[NUM_BOIDS];

global vec3 g_target_positions[NUM_TARGETS];
global vec3 g_obstacle_positions[NUM_OBSTACLES];

static const char *instanced_vs =
    "struct GlobalUniforms {\n"
    "    model: mat4x4<f32>,\n"
    "    view: mat4x4<f32>,\n"
    "    proj: mat4x4<f32>,\n"
    "    view_proj: mat4x4<f32>,\n"
    "};\n"
    "\n"
    "struct InstanceData {\n"
    "    model: mat4x4<f32>,\n"
    "};\n"
    "\n"
    "@group(0) @binding(0) var<uniform> global: GlobalUniforms;\n"
    "@group(0) @binding(1) var<uniform> color: vec4<f32>;\n"
    "@group(1) @binding(0) var<storage, read> instances: array<InstanceData>;\n"
    "\n"
    "struct VertexInput {\n"
    "    @location(0) position: vec3<f32>,\n"
    "    @location(1) normal: vec3<f32>,\n"
    "    @location(2) vertex_color: vec4<f32>,\n"
    "};\n"
    "\n"
    "struct VertexOutput {\n"
    "    @builtin(position) position: vec4<f32>,\n"
    "    @location(0) world_normal: vec3<f32>,\n"
    "    @location(1) material_color: vec4<f32>,\n"
    "};\n"
    "\n"
    "@vertex\n"
    "fn vs_main(@builtin(instance_index) instance_idx: u32, in: VertexInput) "
    "-> VertexOutput {\n"
    "    var out: VertexOutput;\n"
    "    let model = instances[instance_idx].model;\n"
    "    let mvp = global.view_proj * model;\n"
    "    out.position = mvp * vec4<f32>(in.position, 1.0);\n"
    "    let normal_matrix = mat3x3<f32>(model[0].xyz, model[1].xyz, "
    "model[2].xyz);\n"
    "    out.world_normal = normalize(normal_matrix * in.normal);\n"
    "    out.material_color = color;\n"
    "    return out;\n"
    "}\n";

static const char *default_fs =
    "const LIGHT_DIR: vec3<f32> = vec3<f32>(0.5, 0.8, 0.3);\n"
    "const AMBIENT: f32 = 0.15;\n"
    "\n"
    "@fragment\n"
    "fn fs_main(@location(0) world_normal: vec3<f32>, @location(1) "
    "material_color: vec4<f32>) "
    "-> @location(0) vec4<f32> {\n"
    "    let light_dir = normalize(LIGHT_DIR);\n"
    "    let n = normalize(world_normal);\n"
    "    let ndotl = max(dot(n, light_dir), 0.0);\n"
    "    let diffuse = AMBIENT + (1.0 - AMBIENT) * ndotl;\n"
    "    return vec4<f32>(material_color.rgb * diffuse, material_color.a);\n"
    "}\n";

void ecs_world_init_full(EcsWorld *world, ArenaAllocator *arena) {
    ecs_world_init(world, arena);
    ecs_store_init(world);
}

void app_init(AppMemory *memory) {
    UNUSED(memory);

    if (!is_main_thread()) {
        return;
    }

    AppContext *app_ctx = app_ctx_current();

    ecs_world_init_full(&g_world, &app_ctx->arena);

    ECS_COMPONENT(&g_world, Position);
    ECS_COMPONENT(&g_world, Heading);
    ECS_COMPONENT(&g_world, BoidTag);
    ECS_COMPONENT(&g_world, TargetTag);
    ECS_COMPONENT(&g_world, ObstacleTag);

    f32 spawn_radius = 50.0f;
    for (i32 i = 0; i < NUM_BOIDS; i++) {
        EcsEntity e = ecs_entity_new(&g_world);

        f32 theta = ((f32)i / NUM_BOIDS) * 2.0f * 3.14159f * 100.0f;
        f32 phi = ((f32)(i * 7) / NUM_BOIDS) * 3.14159f;
        f32 r = spawn_radius * (0.5f + 0.5f * ((f32)(i % 100) / 100.0f));

        f32 px = r * sinf(phi) * cosf(theta);
        f32 py = r * sinf(phi) * sinf(theta);
        f32 pz = r * cosf(phi);

        f32 hx = -px;
        f32 hy = -py;
        f32 hz = -pz;
        f32 len = sqrtf(hx*hx + hy*hy + hz*hz);
        if (len > 0.001f) {
            hx /= len;
            hy /= len;
            hz /= len;
        } else {
            hx = 1.0f;
            hy = 0.0f;
            hz = 0.0f;
        }

        ecs_set(&g_world, e, Position, { .x = px, .y = py, .z = pz });
        ecs_set(&g_world, e, Heading, { .x = hx, .y = hy, .z = hz });
        ecs_add(&g_world, e, ecs_id(BoidTag));
    }

    for (i32 i = 0; i < NUM_TARGETS; i++) {
        EcsEntity e = ecs_entity_new(&g_world);
        f32 angle = (f32)i * 3.14159f;
        f32 px = 80.0f * cosf(angle);
        f32 py = 0.0f;
        f32 pz = 80.0f * sinf(angle);
        ecs_set(&g_world, e, Position, { .x = px, .y = py, .z = pz });
        ecs_add(&g_world, e, ecs_id(TargetTag));

        g_target_positions[i][0] = px;
        g_target_positions[i][1] = py;
        g_target_positions[i][2] = pz;
    }

    for (i32 i = 0; i < NUM_OBSTACLES; i++) {
        EcsEntity e = ecs_entity_new(&g_world);
        f32 px = 0.0f;
        f32 py = 0.0f;
        f32 pz = 0.0f;
        ecs_set(&g_world, e, Position, { .x = px, .y = py, .z = pz });
        ecs_add(&g_world, e, ecs_id(ObstacleTag));

        g_obstacle_positions[i][0] = px;
        g_obstacle_positions[i][1] = py;
        g_obstacle_positions[i][2] = pz;
    }

    g_camera = camera_init(VEC3(0, 100, 200), VEC3(-0.4f, 0, 0), 45.0f);

    renderer_init(&app_ctx->arena, app_ctx->num_threads);

    g_cube_mesh = renderer_upload_mesh(&(MeshDesc){
        .vertices = cube_vertices,
        .vertex_size = sizeof(cube_vertices),
        .indices = cube_indices,
        .index_size = sizeof(cube_indices),
        .index_count = CUBE_INDEX_COUNT,
        .index_format = GPU_INDEX_FORMAT_U16,
    });

    g_instance_buffer = renderer_create_instance_buffer(&(InstanceBufferDesc){
        .stride = sizeof(mat4),
        .max_instances = NUM_BOIDS,
    });

    g_cube_material = renderer_create_material(&(MaterialDesc){
        .shader_desc = (GpuShaderDesc){
            .vs_code = instanced_vs,
            .fs_code = default_fs,
            .uniform_blocks = {
                {.stage = GPU_STAGE_VERTEX, .size = sizeof(GlobalUniforms), .binding = 0},
                {.stage = GPU_STAGE_VERTEX, .size = sizeof(vec4), .binding = 1},
            },
            .uniform_block_count = 2,
            .storage_buffers = {
                {.stage = GPU_STAGE_VERTEX, .binding = 0, .readonly = true},
            },
            .storage_buffer_count = 1,
        },
        .vertex_layout = {
            .stride = VERTEX_STRIDE,
            .attrs = {
                {GPU_VERTEX_FORMAT_FLOAT3, 0, 0},
                {GPU_VERTEX_FORMAT_FLOAT3, VERTEX_NORMAL_OFFSET, 1},
                {GPU_VERTEX_FORMAT_FLOAT4, VERTEX_COLOR_OFFSET, 2},
            },
            .attr_count = 3,
        },
        .primitive = GPU_PRIMITIVE_TRIANGLES,
        .depth_test = true,
        .depth_write = true,
        .properties = {
            {.name = "color", .type = MAT_PROP_VEC4, .binding = 1},
        },
        .property_count = 1,
    });

    material_set_vec4(g_cube_material, "color", (vec4){0.2f, 0.6f, 1.0f, 1.0f});

    LOG_INFO("Boids demo initialized: % boids", FMT_UINT(NUM_BOIDS));
}

void app_update_and_render(AppMemory *memory) {
    Range_u64 range = lane_range(GRID_SIZE);
    for (u64 i = range.min; i < range.max; i++) {
        g_buckets[i].count = 0;
    }
    lane_sync();

    Range_u64 boid_range = lane_range(NUM_BOIDS);
    for (u64 i = boid_range.min; i < boid_range.max; i++) {
        mat4 *model = &g_instance_data[i];
        mat4_identity(*model);

        f32 x = 50.0f * sinf((f32)i * 0.1f + memory->total_time * 0.5f);
        f32 y = 20.0f * sinf((f32)i * 0.2f + memory->total_time * 0.3f);
        f32 z = 50.0f * cosf((f32)i * 0.15f + memory->total_time * 0.4f);

        mat4_translate(*model, VEC3(x, y, z));
        mat4_scale_uni(*model, 0.5f);
    }
    lane_sync();

    if (is_main_thread()) {
        camera_update(&g_camera, memory->canvas_width, memory->canvas_height);

        renderer_begin_frame(g_camera.view, g_camera.proj,
                             (GpuColor){0.05f, 0.05f, 0.1f, 1.0f});

        renderer_update_instance_buffer(g_instance_buffer, g_instance_data, NUM_BOIDS);
        renderer_draw_mesh_instanced(g_cube_mesh, g_cube_material, g_instance_buffer);

        renderer_end_frame();
    }
}
