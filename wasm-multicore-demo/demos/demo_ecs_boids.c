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

#define NUM_BOIDS 100000
#define NUM_TARGETS 2
#define NUM_OBSTACLES 1

#define GRID_SIZE (8192)
#define MAX_PER_BUCKET 256
#define CELL_SIZE 8.0f

#define BOID_SEPARATION_WEIGHT 1.0f
#define BOID_ALIGNMENT_WEIGHT 1.0f
#define BOID_TARGET_WEIGHT 2.0f
#define BOID_OBSTACLE_AVERSION_DISTANCE 30.0f
#define BOID_MOVE_SPEED 25.0f

typedef struct { f32 x, y, z; } Position;
typedef struct { f32 x, y, z; } Heading;
typedef struct { u32 index; } BoidIndex;
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
    f32 sum_align_x, sum_align_y, sum_align_z;
    f32 sum_sep_x, sum_sep_y, sum_sep_z;
    i32 nearest_target_idx;
    i32 nearest_obstacle_idx;
    f32 nearest_obstacle_dist;
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

global f32 g_total_time = 0.0f;

void ecs_world_init_full(EcsWorld *world, ArenaAllocator *arena) {
    ecs_world_init(world, arena);
    ecs_store_init(world);
}

global i32 g_target_index = 0;

void MoveTargetsSystem(EcsIter *it) {
    Position *positions = ecs_field(it, Position, 0);

    f32 t = g_total_time;

    for (i32 i = 0; i < it->count; i++) {
        i32 idx = g_target_index;
        g_target_index++;

        f32 angle = t * 0.5f + (f32)idx * 3.14159f;
        f32 radius = 80.0f;
        f32 px = radius * cosf(angle);
        f32 py = 20.0f * sinf(t * 0.3f + (f32)idx * 1.5f);
        f32 pz = radius * sinf(angle);

        positions[i].x = px;
        positions[i].y = py;
        positions[i].z = pz;

        if (idx < NUM_TARGETS) {
            g_target_positions[idx][0] = px;
            g_target_positions[idx][1] = py;
            g_target_positions[idx][2] = pz;
        }
    }
}

global i32 g_obstacle_index = 0;

void MoveObstaclesSystem(EcsIter *it) {
    Position *positions = ecs_field(it, Position, 0);

    f32 t = g_total_time;

    for (i32 i = 0; i < it->count; i++) {
        i32 idx = g_obstacle_index;
        g_obstacle_index++;

        f32 px = 40.0f * sinf(t * 0.2f);
        f32 py = 30.0f * sinf(t * 0.15f);
        f32 pz = 40.0f * cosf(t * 0.25f);

        positions[i].x = px;
        positions[i].y = py;
        positions[i].z = pz;

        if (idx < NUM_OBSTACLES) {
            g_obstacle_positions[idx][0] = px;
            g_obstacle_positions[idx][1] = py;
            g_obstacle_positions[idx][2] = pz;
        }
    }
}

void InsertBoidsSystem(EcsIter *it) {
    Position *positions = ecs_field(it, Position, 0);
    Heading *headings = ecs_field(it, Heading, 1);
    BoidIndex *indices = ecs_field(it, BoidIndex, 2);

    for (i32 i = 0; i < it->count; i++) {
        f32 px = positions[i].x;
        f32 py = positions[i].y;
        f32 pz = positions[i].z;

        u32 hash = spatial_hash_3f(px, py, pz, CELL_SIZE) % GRID_SIZE;

        u32 slot = ins_atomic_u32_inc_eval(&g_buckets[hash].count) - 1;

        if (slot < MAX_PER_BUCKET) {
            BoidBucketEntry *entry = &g_buckets[hash].entries[slot];
            entry->px = px;
            entry->py = py;
            entry->pz = pz;
            entry->hx = headings[i].x;
            entry->hy = headings[i].y;
            entry->hz = headings[i].z;
            entry->boid_idx = indices[i].index;
        }
    }
}

void MergeCellsSystem(EcsIter *it) {
    for (i32 i = 0; i < it->count; i++) {
        i32 bucket_idx = it->offset + i;
        BoidBucket *bucket = &g_buckets[bucket_idx];
        if (bucket->count == 0) continue;

        u32 count = bucket->count;
        if (count > MAX_PER_BUCKET) count = MAX_PER_BUCKET;

        f32 sum_ax = 0, sum_ay = 0, sum_az = 0;
        f32 sum_sx = 0, sum_sy = 0, sum_sz = 0;
        for (u32 j = 0; j < count; j++) {
            BoidBucketEntry *entry = &bucket->entries[j];
            sum_ax += entry->hx;
            sum_ay += entry->hy;
            sum_az += entry->hz;
            sum_sx += entry->px;
            sum_sy += entry->py;
            sum_sz += entry->pz;
        }
        bucket->sum_align_x = sum_ax;
        bucket->sum_align_y = sum_ay;
        bucket->sum_align_z = sum_az;
        bucket->sum_sep_x = sum_sx;
        bucket->sum_sep_y = sum_sy;
        bucket->sum_sep_z = sum_sz;

        f32 first_px = bucket->entries[0].px;
        f32 first_py = bucket->entries[0].py;
        f32 first_pz = bucket->entries[0].pz;

        f32 nearest_target_dist_sq = 1e18f;
        i32 nearest_target_idx = 0;
        for (i32 t = 0; t < NUM_TARGETS; t++) {
            f32 dx = g_target_positions[t][0] - first_px;
            f32 dy = g_target_positions[t][1] - first_py;
            f32 dz = g_target_positions[t][2] - first_pz;
            f32 dist_sq = dx*dx + dy*dy + dz*dz;
            if (dist_sq < nearest_target_dist_sq) {
                nearest_target_dist_sq = dist_sq;
                nearest_target_idx = t;
            }
        }
        bucket->nearest_target_idx = nearest_target_idx;

        f32 nearest_obs_dist_sq = 1e18f;
        i32 nearest_obs_idx = 0;
        for (i32 o = 0; o < NUM_OBSTACLES; o++) {
            f32 dx = g_obstacle_positions[o][0] - first_px;
            f32 dy = g_obstacle_positions[o][1] - first_py;
            f32 dz = g_obstacle_positions[o][2] - first_pz;
            f32 dist_sq = dx*dx + dy*dy + dz*dz;
            if (dist_sq < nearest_obs_dist_sq) {
                nearest_obs_dist_sq = dist_sq;
                nearest_obs_idx = o;
            }
        }
        bucket->nearest_obstacle_idx = nearest_obs_idx;
        bucket->nearest_obstacle_dist = sqrtf(nearest_obs_dist_sq);
    }
}

void SteerBoidsSystem(EcsIter *it) {
    Position *positions = ecs_field(it, Position, 0);
    Heading *headings = ecs_field(it, Heading, 1);

    f32 dt = it->delta_time;
    if (dt > 0.05f) dt = 0.05f;

    for (i32 i = 0; i < it->count; i++) {
        f32 px = positions[i].x;
        f32 py = positions[i].y;
        f32 pz = positions[i].z;
        f32 forward_x = headings[i].x;
        f32 forward_y = headings[i].y;
        f32 forward_z = headings[i].z;

        u32 hash = spatial_hash_3f(px, py, pz, CELL_SIZE) % GRID_SIZE;
        BoidBucket *bucket = &g_buckets[hash];
        u32 count = bucket->count;
        if (count > MAX_PER_BUCKET) count = MAX_PER_BUCKET;

        f32 alignment_x, alignment_y, alignment_z;
        f32 separation_x, separation_y, separation_z;
        i32 neighbor_count;

        if (count == 0) {
            neighbor_count = 1;
            alignment_x = forward_x;
            alignment_y = forward_y;
            alignment_z = forward_z;
            separation_x = px;
            separation_y = py;
            separation_z = pz;
        } else {
            neighbor_count = (i32)count;
            alignment_x = bucket->sum_align_x;
            alignment_y = bucket->sum_align_y;
            alignment_z = bucket->sum_align_z;
            separation_x = bucket->sum_sep_x;
            separation_y = bucket->sum_sep_y;
            separation_z = bucket->sum_sep_z;
        }

        f32 inv_count = 1.0f / (f32)neighbor_count;

        i32 nearest_obstacle_idx = bucket->nearest_obstacle_idx;
        f32 nearest_obstacle_x = g_obstacle_positions[nearest_obstacle_idx][0];
        f32 nearest_obstacle_y = g_obstacle_positions[nearest_obstacle_idx][1];
        f32 nearest_obstacle_z = g_obstacle_positions[nearest_obstacle_idx][2];
        f32 nearest_obstacle_dist = bucket->nearest_obstacle_dist;

        i32 nearest_target_idx = bucket->nearest_target_idx;
        f32 nearest_target_x = g_target_positions[nearest_target_idx][0];
        f32 nearest_target_y = g_target_positions[nearest_target_idx][1];
        f32 nearest_target_z = g_target_positions[nearest_target_idx][2];

        f32 avg_align_x = alignment_x * inv_count;
        f32 avg_align_y = alignment_y * inv_count;
        f32 avg_align_z = alignment_z * inv_count;
        f32 align_dx = avg_align_x - forward_x;
        f32 align_dy = avg_align_y - forward_y;
        f32 align_dz = avg_align_z - forward_z;
        f32 align_len = sqrtf(align_dx*align_dx + align_dy*align_dy + align_dz*align_dz);
        f32 align_result_x = 0, align_result_y = 0, align_result_z = 0;
        if (align_len > 0.0001f) {
            f32 inv = BOID_ALIGNMENT_WEIGHT / align_len;
            align_result_x = align_dx * inv;
            align_result_y = align_dy * inv;
            align_result_z = align_dz * inv;
        }

        f32 sep_dx = px * (f32)neighbor_count - separation_x;
        f32 sep_dy = py * (f32)neighbor_count - separation_y;
        f32 sep_dz = pz * (f32)neighbor_count - separation_z;
        f32 sep_len = sqrtf(sep_dx*sep_dx + sep_dy*sep_dy + sep_dz*sep_dz);
        f32 sep_result_x = 0, sep_result_y = 0, sep_result_z = 0;
        if (sep_len > 0.0001f) {
            f32 inv = BOID_SEPARATION_WEIGHT / sep_len;
            sep_result_x = sep_dx * inv;
            sep_result_y = sep_dy * inv;
            sep_result_z = sep_dz * inv;
        }

        f32 target_dx = nearest_target_x - px;
        f32 target_dy = nearest_target_y - py;
        f32 target_dz = nearest_target_z - pz;
        f32 target_len = sqrtf(target_dx*target_dx + target_dy*target_dy + target_dz*target_dz);
        f32 target_result_x = 0, target_result_y = 0, target_result_z = 0;
        if (target_len > 0.0001f) {
            f32 inv = BOID_TARGET_WEIGHT / target_len;
            target_result_x = target_dx * inv;
            target_result_y = target_dy * inv;
            target_result_z = target_dz * inv;
        }

        f32 obs_steer_x = px - nearest_obstacle_x;
        f32 obs_steer_y = py - nearest_obstacle_y;
        f32 obs_steer_z = pz - nearest_obstacle_z;
        f32 obs_steer_len = sqrtf(obs_steer_x*obs_steer_x + obs_steer_y*obs_steer_y + obs_steer_z*obs_steer_z);
        f32 avoid_heading_x = 0, avoid_heading_y = 0, avoid_heading_z = 0;
        if (obs_steer_len > 0.0001f) {
            f32 inv = 1.0f / obs_steer_len;
            f32 norm_x = obs_steer_x * inv;
            f32 norm_y = obs_steer_y * inv;
            f32 norm_z = obs_steer_z * inv;
            avoid_heading_x = (nearest_obstacle_x + norm_x * BOID_OBSTACLE_AVERSION_DISTANCE) - px;
            avoid_heading_y = (nearest_obstacle_y + norm_y * BOID_OBSTACLE_AVERSION_DISTANCE) - py;
            avoid_heading_z = (nearest_obstacle_z + norm_z * BOID_OBSTACLE_AVERSION_DISTANCE) - pz;
        }

        f32 normal_x = align_result_x + sep_result_x + target_result_x;
        f32 normal_y = align_result_y + sep_result_y + target_result_y;
        f32 normal_z = align_result_z + sep_result_z + target_result_z;
        f32 normal_len = sqrtf(normal_x*normal_x + normal_y*normal_y + normal_z*normal_z);
        if (normal_len > 0.0001f) {
            f32 inv = 1.0f / normal_len;
            normal_x *= inv;
            normal_y *= inv;
            normal_z *= inv;
        } else {
            normal_x = forward_x;
            normal_y = forward_y;
            normal_z = forward_z;
        }

        f32 target_forward_x, target_forward_y, target_forward_z;
        f32 obstacle_dist_from_radius = nearest_obstacle_dist - BOID_OBSTACLE_AVERSION_DISTANCE;
        if (obstacle_dist_from_radius < 0) {
            target_forward_x = avoid_heading_x;
            target_forward_y = avoid_heading_y;
            target_forward_z = avoid_heading_z;
        } else {
            target_forward_x = normal_x;
            target_forward_y = normal_y;
            target_forward_z = normal_z;
        }

        f32 new_hx = forward_x + dt * (target_forward_x - forward_x);
        f32 new_hy = forward_y + dt * (target_forward_y - forward_y);
        f32 new_hz = forward_z + dt * (target_forward_z - forward_z);
        f32 new_len = sqrtf(new_hx*new_hx + new_hy*new_hy + new_hz*new_hz);
        if (new_len > 0.0001f) {
            f32 inv = 1.0f / new_len;
            new_hx *= inv;
            new_hy *= inv;
            new_hz *= inv;
        }

        f32 move_dist = BOID_MOVE_SPEED * dt;
        positions[i].x = px + new_hx * move_dist;
        positions[i].y = py + new_hy * move_dist;
        positions[i].z = pz + new_hz * move_dist;
        headings[i].x = new_hx;
        headings[i].y = new_hy;
        headings[i].z = new_hz;
    }
}

void BuildMatricesSystem(EcsIter *it) {
    Position *positions = ecs_field(it, Position, 0);
    Heading *headings = ecs_field(it, Heading, 1);
    BoidIndex *indices = ecs_field(it, BoidIndex, 2);

    for (i32 i = 0; i < it->count; i++) {
        u32 idx = indices[i].index;
        mat4 *model = &g_instance_data[idx];

        vec3 pos = { positions[i].x, positions[i].y, positions[i].z };
        vec3 dir = { headings[i].x, headings[i].y, headings[i].z };

        quaternion rot;
        quat_look_at_dir(dir, rot);

        vec3 scale = { 0.5f, 0.5f, 1.0f };
        mat_trs(pos, rot, scale, *model);
    }
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
    ECS_COMPONENT(&g_world, BoidIndex);
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
        ecs_set(&g_world, e, BoidIndex, { .index = (u32)i });
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

    EcsTerm move_targets_terms[] = {
        ecs_term_inout(ecs_id(Position)),
        ecs_term_none(ecs_id(TargetTag)),
    };
    ecs_system_init(&g_world, &(EcsSystemDesc){
        .terms = move_targets_terms,
        .term_count = 2,
        .callback = MoveTargetsSystem,
        .name = "MoveTargetsSystem",
    });

    EcsTerm move_obstacles_terms[] = {
        ecs_term_inout(ecs_id(Position)),
        ecs_term_none(ecs_id(ObstacleTag)),
    };
    ecs_system_init(&g_world, &(EcsSystemDesc){
        .terms = move_obstacles_terms,
        .term_count = 2,
        .callback = MoveObstaclesSystem,
        .name = "MoveObstaclesSystem",
    });

    EcsTerm insert_boids_terms[] = {
        ecs_term_in(ecs_id(Position)),
        ecs_term_in(ecs_id(Heading)),
        ecs_term_in(ecs_id(BoidIndex)),
        ecs_term_none(ecs_id(BoidTag)),
    };
    EcsSystem *insert_boids_sys = ecs_system_init(&g_world, &(EcsSystemDesc){
        .terms = insert_boids_terms,
        .term_count = 4,
        .callback = InsertBoidsSystem,
        .name = "InsertBoidsSystem",
        .sync_mode = ECS_SYNC_BARRIER,
    });

    EcsSystem *merge_cells_sys = ecs_system_init(&g_world, &(EcsSystemDesc){
        .iter_mode = ECS_ITER_RANGE,
        .iter_count = GRID_SIZE,
        .callback = MergeCellsSystem,
        .name = "MergeCellsSystem",
        .sync_mode = ECS_SYNC_BARRIER,
    });
    ecs_system_depends_on(merge_cells_sys, insert_boids_sys);

    EcsTerm steer_boids_terms[] = {
        ecs_term_inout(ecs_id(Position)),
        ecs_term_inout(ecs_id(Heading)),
        ecs_term_none(ecs_id(BoidTag)),
    };
    EcsSystem *steer_boids_sys = ecs_system_init(&g_world, &(EcsSystemDesc){
        .terms = steer_boids_terms,
        .term_count = 3,
        .callback = SteerBoidsSystem,
        .name = "SteerBoidsSystem",
    });
    ecs_system_depends_on(steer_boids_sys, merge_cells_sys);

    EcsTerm build_matrices_terms[] = {
        ecs_term_in(ecs_id(Position)),
        ecs_term_in(ecs_id(Heading)),
        ecs_term_in(ecs_id(BoidIndex)),
        ecs_term_none(ecs_id(BoidTag)),
    };
    ecs_system_init(&g_world, &(EcsSystemDesc){
        .terms = build_matrices_terms,
        .term_count = 4,
        .callback = BuildMatricesSystem,
        .name = "BuildMatricesSystem",
    });

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
    g_total_time = memory->total_time;
    g_target_index = 0;
    g_obstacle_index = 0;

    Range_u64 range = lane_range(GRID_SIZE);
    for (u64 i = range.min; i < range.max; i++) {
        g_buckets[i].count = 0;
    }

    ecs_progress(&g_world, memory->dt);

    if (is_main_thread()) {
        camera_update(&g_camera, memory->canvas_width, memory->canvas_height);

        renderer_begin_frame(g_camera.view, g_camera.proj,
                             (GpuColor){0.05f, 0.05f, 0.1f, 1.0f});

        renderer_update_instance_buffer(g_instance_buffer, g_instance_data, NUM_BOIDS);
        renderer_draw_mesh_instanced(g_cube_mesh, g_cube_material, g_instance_buffer);

        renderer_end_frame();
    }
}
