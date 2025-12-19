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
global EcsQuery g_move_query;
global EcsQuery g_render_query;
global EcsEntity g_test_entity;
global i32 g_frame_count;
global EcsEntity g_comp_position;
global EcsEntity g_comp_velocity;
global EcsEntity g_comp_health;

void ecs_world_init_full(EcsWorld *world, ArenaAllocator *arena) {
    ecs_world_init(world, arena);
    ecs_store_init(world);
}

internal void set_position(EcsWorld *world, EcsEntity entity, f32 x, f32 y) {
    Position p = { x, y };
    ecs_set_ptr(world, entity, g_comp_position, &p);
}

internal void set_velocity(EcsWorld *world, EcsEntity entity, f32 x, f32 y) {
    Velocity v = { x, y };
    ecs_set_ptr(world, entity, g_comp_velocity, &v);
}

void app_init(AppMemory *memory) {
    UNUSED(memory);

    if (!is_main_thread()) {
        return;
    }

    AppContext *app_ctx = app_ctx_current();

    ecs_world_init_full(&g_world, &app_ctx->arena);
    LOG_INFO("ECS World initialized");

    LOG_INFO("=== Change Detection Test ===");

    g_comp_position = ecs_component_register(&g_world, sizeof(Position), _Alignof(Position), "Position");
    g_comp_velocity = ecs_component_register(&g_world, sizeof(Velocity), _Alignof(Velocity), "Velocity");
    g_comp_health = ecs_component_register(&g_world, sizeof(Health), _Alignof(Health), "Health");

    LOG_INFO("--- Create entities ---");
    for (i32 i = 0; i < 3; i++) {
        EcsEntity e = ecs_entity_new(&g_world);
        set_position(&g_world, e, (f32)(i * 10), (f32)(i * 10));
        set_velocity(&g_world, e, 1.0f, 1.0f);
    }
    g_test_entity = ecs_entity_new(&g_world);
    set_position(&g_world, g_test_entity, 100.0f, 100.0f);
    set_velocity(&g_world, g_test_entity, 2.0f, 2.0f);

    LOG_INFO("Created 4 entities with [Position, Velocity]");

    LOG_INFO("--- Create cached query with In/Out fields ---");
    EcsTerm move_terms[] = {
        ecs_term_inout(g_comp_position),
        ecs_term_in(g_comp_velocity),
    };
    ecs_query_init_terms(&g_move_query, &g_world, move_terms, 2);
    ecs_query_cache_init(&g_move_query);

    LOG_INFO("Move query: Position [inout], Velocity [in]");
    LOG_INFO("  read_fields: %", FMT_UINT(g_move_query.read_fields));
    LOG_INFO("  write_fields: %", FMT_UINT(g_move_query.write_fields));

    EcsTerm render_terms[] = {
        ecs_term_in(g_comp_position),
    };
    ecs_query_init_terms(&g_render_query, &g_world, render_terms, 1);
    ecs_query_cache_init(&g_render_query);

    LOG_INFO("Render query: Position [in]");

    LOG_INFO("--- Initial state ---");
    LOG_INFO("Move query changed: %", FMT_UINT(ecs_query_changed(&g_move_query)));
    LOG_INFO("Render query changed: %", FMT_UINT(ecs_query_changed(&g_render_query)));

    LOG_INFO("--- Sync queries (mark as processed) ---");
    ecs_query_sync(&g_move_query);
    ecs_query_sync(&g_render_query);

    LOG_INFO("Move query changed: %", FMT_UINT(ecs_query_changed(&g_move_query)));
    LOG_INFO("Render query changed: %", FMT_UINT(ecs_query_changed(&g_render_query)));

    LOG_INFO("--- Modify Position of test entity ---");
    set_position(&g_world, g_test_entity, 200.0f, 200.0f);

    LOG_INFO("Move query changed: % (Position is read)", FMT_UINT(ecs_query_changed(&g_move_query)));
    LOG_INFO("Render query changed: % (Position is read)", FMT_UINT(ecs_query_changed(&g_render_query)));

    LOG_INFO("--- Sync and modify Velocity ---");
    ecs_query_sync(&g_move_query);
    ecs_query_sync(&g_render_query);

    set_velocity(&g_world, g_test_entity, 5.0f, 5.0f);

    LOG_INFO("Move query changed: % (Velocity is read)", FMT_UINT(ecs_query_changed(&g_move_query)));
    LOG_INFO("Render query changed: % (Velocity NOT read)", FMT_UINT(ecs_query_changed(&g_render_query)));

    LOG_INFO("--- Sync and add new entity ---");
    ecs_query_sync(&g_move_query);

    EcsEntity new_e = ecs_entity_new(&g_world);
    set_position(&g_world, new_e, 0.0f, 0.0f);
    set_velocity(&g_world, new_e, 1.0f, 1.0f);

    LOG_INFO("Move query changed: % (entity added to table)", FMT_UINT(ecs_query_changed(&g_move_query)));

    LOG_INFO("--- Per-table change detection ---");
    ecs_query_sync(&g_move_query);

    EcsIter it = ecs_query_iter(&g_move_query);
    while (ecs_iter_next(&it)) {
        b32 changed = ecs_iter_changed(&it);
        LOG_INFO("Table %: changed=%", FMT_UINT(it.table->id), FMT_UINT(changed));
    }

    LOG_INFO("--- Modify one entity, check per-table ---");
    set_position(&g_world, g_test_entity, 300.0f, 300.0f);

    it = ecs_query_iter(&g_move_query);
    while (ecs_iter_next(&it)) {
        b32 changed = ecs_iter_changed(&it);
        LOG_INFO("Table %: changed=%", FMT_UINT(it.table->id), FMT_UINT(changed));
        if (changed) {
            LOG_INFO("  Processing changed table...");
            ecs_iter_sync(&it);
        }
    }

    LOG_INFO("--- Check again after selective sync ---");
    it = ecs_query_iter(&g_move_query);
    while (ecs_iter_next(&it)) {
        b32 changed = ecs_iter_changed(&it);
        LOG_INFO("Table %: changed=%", FMT_UINT(it.table->id), FMT_UINT(changed));
    }

    LOG_INFO("=== Change Detection Tests Complete ===");
    g_frame_count = 0;
}

void app_update_and_render(AppMemory *memory) {
    UNUSED(memory);

    if (!is_main_thread()) {
        return;
    }

    g_frame_count++;

    if (g_frame_count == 60) {
        LOG_INFO("Frame 60: Modifying entity...");
        set_position(&g_world, g_test_entity, 500.0f, 500.0f);
    }

    if (ecs_query_changed(&g_move_query)) {
        EcsIter it = ecs_query_iter(&g_move_query);
        while (ecs_iter_next(&it)) {
            Position *p = (Position*)ecs_iter_field(&it, 0);
            Velocity *v = (Velocity*)ecs_iter_field(&it, 1);

            for (i32 i = 0; i < it.count; i++) {
                p[i].x += v[i].x * 0.016f;
                p[i].y += v[i].y * 0.016f;
            }
        }
        ecs_query_sync(&g_move_query);

        if (g_frame_count <= 5 || g_frame_count == 60 || g_frame_count == 61) {
            LOG_INFO("Frame %: Move system ran (query was dirty)", FMT_UINT(g_frame_count));
        }
    } else {
        if (g_frame_count <= 5 || g_frame_count == 59 || g_frame_count == 62) {
            LOG_INFO("Frame %: Move system SKIPPED (no changes)", FMT_UINT(g_frame_count));
        }
    }
}
