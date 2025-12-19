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
    LOG_INFO("ECS World initialized");

    LOG_INFO("=== Query Cache Test ===");

    ECS_COMPONENT(&g_world, Position);
    ECS_COMPONENT(&g_world, Velocity);
    ECS_COMPONENT(&g_world, Health);

    LOG_INFO("--- Create initial entities BEFORE caching query ---");
    for (i32 i = 0; i < 3; i++) {
        EcsEntity e = ecs_entity_new(&g_world);
        ecs_set(&g_world, e, Position, { .x = (f32)(i * 10), .y = (f32)(i * 10) });
        ecs_set(&g_world, e, Velocity, { .x = 1.0f, .y = 1.0f });
    }
    LOG_INFO("Created 3 entities with [Position, Velocity]");

    LOG_INFO("--- Create cached query for [Position, Velocity] ---");
    EcsEntity terms[] = { ecs_id(Position), ecs_id(Velocity) };
    ecs_query_init(&g_move_query, &g_world, terms, 2);
    ecs_query_cache_init(&g_move_query);

    LOG_INFO("Query cached: % matches", FMT_UINT(g_move_query.cache.match_count));
    LOG_INFO("World has % cached queries", FMT_UINT(g_world.cached_query_count));

    LOG_INFO("--- Iterate cached query ---");
    EcsIter it = ecs_query_iter(&g_move_query);
    i32 total = 0;
    while (ecs_iter_next(&it)) {
        Position *p = ecs_field(&it, Position, 0);
        Velocity *v = ecs_field(&it, Velocity, 1);

        LOG_INFO("  Table %: % entities", FMT_UINT(it.table->id), FMT_UINT(it.count));
        for (i32 i = 0; i < it.count; i++) {
            LOG_INFO("    pos=(%, %), vel=(%, %)",
                     FMT_UINT((u32)p[i].x), FMT_UINT((u32)p[i].y),
                     FMT_UINT((u32)v[i].x), FMT_UINT((u32)v[i].y));
        }
        total += it.count;
    }
    LOG_INFO("Total from cached query: % entities", FMT_UINT(total));

    LOG_INFO("--- Create MORE entities AFTER query is cached ---");
    for (i32 i = 0; i < 4; i++) {
        EcsEntity e = ecs_entity_new(&g_world);
        ecs_set(&g_world, e, Position, { .x = (f32)(100 + i * 10), .y = (f32)(100 + i * 10) });
        ecs_set(&g_world, e, Velocity, { .x = 2.0f, .y = 2.0f });
    }
    LOG_INFO("Created 4 more entities with [Position, Velocity]");

    LOG_INFO("Query cache now has: % matches", FMT_UINT(g_move_query.cache.match_count));

    LOG_INFO("--- Iterate cached query again (should include new entities) ---");
    it = ecs_query_iter(&g_move_query);
    total = 0;
    while (ecs_iter_next(&it)) {
        Position *p = ecs_field(&it, Position, 0);
        Velocity *v = ecs_field(&it, Velocity, 1);

        LOG_INFO("  Table %: % entities", FMT_UINT(it.table->id), FMT_UINT(it.count));
        for (i32 i = 0; i < it.count; i++) {
            LOG_INFO("    pos=(%, %), vel=(%, %)",
                     FMT_UINT((u32)p[i].x), FMT_UINT((u32)p[i].y),
                     FMT_UINT((u32)v[i].x), FMT_UINT((u32)v[i].y));
        }
        total += it.count;
    }
    LOG_INFO("Total from cached query: % entities", FMT_UINT(total));

    LOG_INFO("--- Create entities with DIFFERENT archetype ---");
    for (i32 i = 0; i < 2; i++) {
        EcsEntity e = ecs_entity_new(&g_world);
        ecs_set(&g_world, e, Position, { .x = (f32)(200 + i * 10), .y = (f32)(200 + i * 10) });
        ecs_set(&g_world, e, Velocity, { .x = 3.0f, .y = 3.0f });
        ecs_set(&g_world, e, Health, { .value = 100.0f });
    }
    LOG_INFO("Created 2 entities with [Position, Velocity, Health]");

    LOG_INFO("Query cache now has: % matches (new archetype added)", FMT_UINT(g_move_query.cache.match_count));

    LOG_INFO("--- Final iteration (should include all matching entities) ---");
    it = ecs_query_iter(&g_move_query);
    total = 0;
    while (ecs_iter_next(&it)) {
        LOG_INFO("  Table %: % entities", FMT_UINT(it.table->id), FMT_UINT(it.count));
        total += it.count;
    }
    LOG_INFO("Total from cached query: % entities (expected 9)", FMT_UINT(total));

    LOG_INFO("--- Create non-matching entities (should NOT affect cache) ---");
    for (i32 i = 0; i < 5; i++) {
        EcsEntity e = ecs_entity_new(&g_world);
        ecs_set(&g_world, e, Position, { .x = 0.0f, .y = 0.0f });
    }
    LOG_INFO("Created 5 entities with [Position] only (no Velocity)");

    LOG_INFO("Query cache still has: % matches (non-matching table ignored)", FMT_UINT(g_move_query.cache.match_count));

    LOG_INFO("=== Query Cache Tests Complete ===");
}

void app_update_and_render(AppMemory *memory) {
    UNUSED(memory);

    if (!is_main_thread()) {
        return;
    }

    EcsIter it = ecs_query_iter(&g_move_query);
    while (ecs_iter_next(&it)) {
        Position *p = ecs_field(&it, Position, 0);
        Velocity *v = ecs_field(&it, Velocity, 1);

        for (i32 i = 0; i < it.count; i++) {
            p[i].x += v[i].x * 0.016f;
            p[i].y += v[i].y * 0.016f;
        }
    }
}
