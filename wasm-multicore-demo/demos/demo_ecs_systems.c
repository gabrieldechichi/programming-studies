#include "context.h"
#include "lib/thread_context.h"
#include "lib/typedefs.h"
#include "os/os.h"
#include "lib/math.h"
#include "cube.h"
#include "renderer.h"
#include "camera.h"
#include "app.h"

#include "ecs/ecs_entity.c"
#include "ecs/ecs_table.c"

typedef struct {
    f32 x;
    f32 y;
} Position;

typedef struct {
    f32 x;
    f32 y;
} Velocity;

typedef struct {
    f32 value;
} Health;

global EcsWorld g_world;
global EcsSystem *g_move_system;
global EcsSystem *g_damage_system;
global EcsSystem *g_print_system;
global u32 g_frame_count = 0;

void ecs_world_init_full(EcsWorld *world, ArenaAllocator *arena) {
    ecs_world_init(world, arena);
    ecs_store_init(world);
}

void MoveSystem(EcsIter *it) {
    LOG_INFO("Move System running");
    Position *p = ecs_field(it, Position, 0);
    Velocity *v = ecs_field(it, Velocity, 1);

    for (i32 i = 0; i < it->count; i++) {
        p[i].x += v[i].x * it->delta_time;
        p[i].y += v[i].y * it->delta_time;
    }
}

void DamageSystem(EcsIter *it) {
    LOG_INFO("Damage System running");
    Health *h = ecs_field(it, Health, 0);
    Velocity *v = ecs_field(it, Velocity, 1);

    for (i32 i = 0; i < it->count; i++) {
        f32 speed = sqrtf(v[i].x * v[i].x + v[i].y * v[i].y);
        h[i].value -= speed * it->delta_time * 0.1f;
    }
}

void PrintSystem(EcsIter *it) {
    Position *p = ecs_field(it, Position, 0);
    LOG_INFO("Print system running");

    // ThreadContext *tctx = tctx_current();
    // for (i32 i = 0; i < it->count; i++) {
    //     if (i == 0) {
    //         LOG_INFO("Thread %: pos[0]=(%, %)",
    //                  FMT_UINT(tctx->thread_idx),
    //                  FMT_UINT((u32)(p[i].x * 100)),
    //                  FMT_UINT((u32)(p[i].y * 100)));
    //     }
    // }
}

void app_init(AppMemory *memory) {
    UNUSED(memory);

    if (!is_main_thread()) {
        return;
    }

    AppContext *app_ctx = app_ctx_current();
    ThreadContext *tctx = tctx_current();

    ecs_world_init_full(&g_world, &app_ctx->arena);
    LOG_INFO("ECS World initialized");

    LOG_INFO("=== ECS Systems Test ===");
    LOG_INFO("Thread count: %", FMT_UINT(tctx->thread_count));

    ECS_COMPONENT(&g_world, Position);
    ECS_COMPONENT(&g_world, Velocity);
    ECS_COMPONENT(&g_world, Health);

    LOG_INFO("--- Creating entities ---");
    for (i32 i = 0; i < 1000; i++) {
        EcsEntity e = ecs_entity_new(&g_world);
        ecs_set(&g_world, e, Position, { .x = (f32)(i % 100), .y = (f32)(i / 100) });
        ecs_set(&g_world, e, Velocity, { .x = 1.0f + (f32)(i % 5), .y = 0.5f + (f32)(i % 3) });
        ecs_set(&g_world, e, Health, { .value = 100.0f });
    }
    LOG_INFO("Created 1000 entities with [Position, Velocity, Health]");

    LOG_INFO("--- Registering systems ---");

    EcsTerm move_terms[] = {
        ecs_term_inout(ecs_id(Position)),
        ecs_term_in(ecs_id(Velocity)),
    };
    g_move_system = ECS_SYSTEM(&g_world, MoveSystem, move_terms, 2);
    LOG_INFO("Registered MoveSystem (Position InOut, Velocity In)");

    EcsTerm damage_terms[] = {
        ecs_term_inout(ecs_id(Health)),
        ecs_term_in(ecs_id(Velocity)),
    };
    g_damage_system = ECS_SYSTEM(&g_world, DamageSystem, damage_terms, 2);
    LOG_INFO("Registered DamageSystem (Health InOut, Velocity In)");

    EcsTerm print_terms[] = {
        ecs_term_in(ecs_id(Position)),
    };
    g_print_system = ECS_SYSTEM(&g_world, PrintSystem, print_terms, 1);
    LOG_INFO("Registered PrintSystem (Position In)");

    LOG_INFO("--- Computing automatic dependencies ---");
    ecs_world_compute_system_dependencies(&g_world);

    LOG_INFO("MoveSystem dependencies: %", FMT_UINT(g_move_system->depends_on_count));
    LOG_INFO("DamageSystem dependencies: %", FMT_UINT(g_damage_system->depends_on_count));
    LOG_INFO("PrintSystem dependencies: %", FMT_UINT(g_print_system->depends_on_count));

    LOG_INFO("Total systems: %", FMT_UINT(g_world.system_count));
    LOG_INFO("=== Initialization complete ===");
}

void app_update_and_render(AppMemory *memory) {
    UNUSED(memory);
    if(is_main_thread()){
        LOG_INFO("------ FRAME START ------");
    }

    f32 delta_time = 0.016f;

    ecs_progress(&g_world, delta_time);

    if (is_main_thread()) {
        g_frame_count++;
        if (g_frame_count % 60 == 0) {
            LOG_INFO("Frame %", FMT_UINT(g_frame_count));
        }

        LOG_INFO("------ FRAME END ------");
    }
}
