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

typedef struct {
    f32 radius;
} Collider;

global EcsWorld g_world;

void ecs_world_init_full(EcsWorld *world, ArenaAllocator *arena) {
    ecs_world_init(world, arena);
    ecs_store_init(world);
}

internal const char* inout_to_string(i16 inout) {
    switch (inout) {
        case EcsInOutDefault: return "InOutDefault";
        case EcsIn: return "In";
        case EcsOut: return "Out";
        case EcsInOut: return "InOut";
        case EcsInOutNone: return "InOutNone";
        default: return "Unknown";
    }
}

internal void print_query_access(EcsQuery *query, const char *name) {
    LOG_INFO("Query '%': % terms, % fields", FMT_STR(name), FMT_UINT(query->term_count), FMT_UINT(query->field_count));
    LOG_INFO("  read_fields:  0x%", FMT_UINT(query->read_fields));
    LOG_INFO("  write_fields: 0x%", FMT_UINT(query->write_fields));

    for (i32 i = 0; i < query->term_count; i++) {
        EcsTerm *term = &query->terms[i];
        if (term->field_index >= 0) {
            u32 field_bit = 1u << term->field_index;
            b32 reads = (query->read_fields & field_bit) != 0;
            b32 writes = (query->write_fields & field_bit) != 0;
            LOG_INFO("  field %: inout=%, reads=%, writes=%",
                     FMT_UINT(term->field_index),
                     FMT_STR(inout_to_string(term->inout)),
                     FMT_UINT(reads), FMT_UINT(writes));
        }
    }
}

void app_init(AppMemory *memory) {
    UNUSED(memory);

    if (!is_main_thread()) {
        return;
    }

    AppContext *app_ctx = app_ctx_current();

    ecs_world_init_full(&g_world, &app_ctx->arena);
    LOG_INFO("ECS World initialized");

    LOG_INFO("=== In/Out/InOut Access Modifiers Test ===");

    ECS_COMPONENT(&g_world, Position);
    ECS_COMPONENT(&g_world, Velocity);
    ECS_COMPONENT(&g_world, Health);
    ECS_COMPONENT(&g_world, Collider);

    for (i32 i = 0; i < 5; i++) {
        EcsEntity e = ecs_entity_new(&g_world);
        ecs_set(&g_world, e, Position, { .x = (f32)(i * 10), .y = (f32)(i * 10) });
        ecs_set(&g_world, e, Velocity, { .x = 1.0f, .y = 1.0f });
        ecs_set(&g_world, e, Health, { .value = 100.0f });
        ecs_set(&g_world, e, Collider, { .radius = 5.0f });
    }
    LOG_INFO("Created 5 entities with [Position, Velocity, Health, Collider]");

    LOG_INFO("");
    LOG_INFO("--- Query 1: Movement System (writes Position, reads Velocity) ---");
    EcsTerm move_terms[] = {
        ecs_term_out(ecs_id(Position)),
        ecs_term_in(ecs_id(Velocity)),
    };
    EcsQuery move_query;
    ecs_query_init_terms(&move_query, &g_world, move_terms, 2);
    print_query_access(&move_query, "MoveSystem");

    LOG_INFO("");
    LOG_INFO("--- Query 2: Render System (reads Position, reads Sprite - read only) ---");
    EcsTerm render_terms[] = {
        ecs_term_in(ecs_id(Position)),
        ecs_term_in(ecs_id(Collider)),
    };
    EcsQuery render_query;
    ecs_query_init_terms(&render_query, &g_world, render_terms, 2);
    print_query_access(&render_query, "RenderSystem");

    LOG_INFO("");
    LOG_INFO("--- Query 3: Collision System (reads+writes Position) ---");
    EcsTerm collision_terms[] = {
        ecs_term_inout(ecs_id(Position)),
        ecs_term_in(ecs_id(Collider)),
    };
    EcsQuery collision_query;
    ecs_query_init_terms(&collision_query, &g_world, collision_terms, 2);
    print_query_access(&collision_query, "CollisionSystem");

    LOG_INFO("");
    LOG_INFO("--- Query 4: Health Filter (no data access, just filter) ---");
    EcsTerm filter_terms[] = {
        ecs_term_in(ecs_id(Position)),
        ecs_term_none(ecs_id(Health)),
    };
    EcsQuery filter_query;
    ecs_query_init_terms(&filter_query, &g_world, filter_terms, 2);
    print_query_access(&filter_query, "HealthFilter");

    LOG_INFO("");
    LOG_INFO("--- Query 5: Default access (InOutDefault -> InOut) ---");
    EcsTerm default_terms[] = {
        ecs_term(ecs_id(Position)),
        ecs_term(ecs_id(Velocity)),
    };
    EcsQuery default_query;
    ecs_query_init_terms(&default_query, &g_world, default_terms, 2);
    print_query_access(&default_query, "DefaultAccess");

    LOG_INFO("");
    LOG_INFO("--- Query 6: Mixed access with optional ---");
    EcsTerm mixed_terms[] = {
        ecs_term_out(ecs_id(Position)),
        ecs_term_in(ecs_id(Velocity)),
        ecs_term_optional(ecs_id(Health)),
    };
    EcsQuery mixed_query;
    ecs_query_init_terms(&mixed_query, &g_world, mixed_terms, 3);
    print_query_access(&mixed_query, "MixedAccess");

    LOG_INFO("");
    LOG_INFO("--- Iterate move_query (Out Position, In Velocity) ---");
    EcsIter it = ecs_query_iter(&move_query);
    while (ecs_iter_next(&it)) {
        Position *p = ecs_field(&it, Position, 0);
        Velocity *v = ecs_field(&it, Velocity, 1);

        for (i32 i = 0; i < it.count; i++) {
            p[i].x += v[i].x;
            p[i].y += v[i].y;
        }
        LOG_INFO("  Moved % entities", FMT_UINT(it.count));
    }

    LOG_INFO("");
    LOG_INFO("--- Verify positions updated ---");
    EcsIter verify_it = ecs_query_iter(&render_query);
    while (ecs_iter_next(&verify_it)) {
        Position *p = ecs_field(&verify_it, Position, 0);

        for (i32 i = 0; i < verify_it.count; i++) {
            LOG_INFO("  Entity %: pos=(%, %)",
                     FMT_UINT(ecs_entity_index(verify_it.entities[i])),
                     FMT_UINT((u32)p[i].x), FMT_UINT((u32)p[i].y));
        }
    }

    LOG_INFO("");
    LOG_INFO("=== In/Out/InOut Tests Complete ===");
}

void app_update_and_render(AppMemory *memory) {
    UNUSED(memory);
}
