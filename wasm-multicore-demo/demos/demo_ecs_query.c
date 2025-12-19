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
    f32 damage;
} Attack;

typedef struct {
    b32 frozen;
} Frozen;

typedef struct {
    f32 mana;
} Mana;

typedef struct {
    f32 stamina;
} Stamina;

global EcsWorld g_world;

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

    LOG_INFO("=== Query Term Operators Test ===");

    ECS_COMPONENT(&g_world, Position);
    ECS_COMPONENT(&g_world, Velocity);
    ECS_COMPONENT(&g_world, Health);
    ECS_COMPONENT(&g_world, Attack);
    ECS_COMPONENT(&g_world, Frozen);
    ECS_COMPONENT(&g_world, Mana);
    ECS_COMPONENT(&g_world, Stamina);

    LOG_INFO("--- Create test entities ---");

    for (i32 i = 0; i < 3; i++) {
        EcsEntity e = ecs_entity_new(&g_world);
        ecs_set(&g_world, e, Position, { .x = (f32)(i * 10), .y = (f32)(i * 10) });
        ecs_set(&g_world, e, Velocity, { .x = (f32)(i + 1), .y = (f32)(i + 1) });
    }
    LOG_INFO("Created 3 entities with [Position, Velocity]");

    for (i32 i = 0; i < 2; i++) {
        EcsEntity e = ecs_entity_new(&g_world);
        ecs_set(&g_world, e, Position, { .x = (f32)(100 + i * 10), .y = (f32)(100 + i * 10) });
        ecs_set(&g_world, e, Velocity, { .x = (f32)(i + 1), .y = (f32)(i + 1) });
        ecs_set(&g_world, e, Health, { .value = (f32)(100 - i * 10) });
    }
    LOG_INFO("Created 2 entities with [Position, Velocity, Health]");

    for (i32 i = 0; i < 2; i++) {
        EcsEntity e = ecs_entity_new(&g_world);
        ecs_set(&g_world, e, Position, { .x = (f32)(200 + i * 10), .y = (f32)(200 + i * 10) });
        ecs_set(&g_world, e, Velocity, { .x = 0.0f, .y = 0.0f });
        ecs_set(&g_world, e, Frozen, { .frozen = true });
    }
    LOG_INFO("Created 2 entities with [Position, Velocity, Frozen]");

    for (i32 i = 0; i < 2; i++) {
        EcsEntity e = ecs_entity_new(&g_world);
        ecs_set(&g_world, e, Position, { .x = (f32)(300 + i * 10), .y = (f32)(300 + i * 10) });
        ecs_set(&g_world, e, Mana, { .mana = (f32)(50 + i * 25) });
    }
    LOG_INFO("Created 2 entities with [Position, Mana]");

    for (i32 i = 0; i < 2; i++) {
        EcsEntity e = ecs_entity_new(&g_world);
        ecs_set(&g_world, e, Position, { .x = (f32)(400 + i * 10), .y = (f32)(400 + i * 10) });
        ecs_set(&g_world, e, Stamina, { .stamina = (f32)(100 + i * 10) });
    }
    LOG_INFO("Created 2 entities with [Position, Stamina]");

    LOG_INFO("Total entities: %", FMT_UINT(ecs_entity_count(&g_world)));
    LOG_INFO("Total tables: %", FMT_UINT(g_world.store.table_count));

    LOG_INFO("");
    LOG_INFO("=== Test 1: Basic AND query (Position, Velocity) ===");
    {
        EcsEntity terms[] = { ecs_id(Position), ecs_id(Velocity) };
        EcsQuery query;
        ecs_query_init(&query, &g_world, terms, 2);

        EcsIter it = ecs_query_iter(&query);
        i32 total = 0;
        while (ecs_iter_next(&it)) {
            Position *p = ecs_field(&it, Position, 0);
            Velocity *v = ecs_field(&it, Velocity, 1);
            LOG_INFO("  Table %: % entities", FMT_UINT(it.table->id), FMT_UINT(it.count));
            for (i32 i = 0; i < it.count; i++) {
                LOG_INFO("    e%: pos=(%, %), vel=(%, %)",
                         FMT_UINT(ecs_entity_index(it.entities[i])),
                         FMT_UINT((u32)p[i].x), FMT_UINT((u32)p[i].y),
                         FMT_UINT((u32)v[i].x), FMT_UINT((u32)v[i].y));
            }
            total += it.count;
        }
        LOG_INFO("Matched % entities (expected 7)", FMT_UINT(total));
    }

    LOG_INFO("");
    LOG_INFO("=== Test 2: NOT query (Position, Velocity, !Frozen) ===");
    {
        EcsTerm terms[] = {
            ecs_term(ecs_id(Position)),
            ecs_term(ecs_id(Velocity)),
            ecs_term_not(ecs_id(Frozen)),
        };
        EcsQuery query;
        ecs_query_init_terms(&query, &g_world, terms, 3);

        LOG_INFO("Query has % terms, % fields", FMT_UINT(query.term_count), FMT_UINT(query.field_count));

        EcsIter it = ecs_query_iter(&query);
        i32 total = 0;
        while (ecs_iter_next(&it)) {
            Position *p = ecs_field(&it, Position, 0);
            Velocity *v = ecs_field(&it, Velocity, 1);
            LOG_INFO("  Table %: % entities", FMT_UINT(it.table->id), FMT_UINT(it.count));
            for (i32 i = 0; i < it.count; i++) {
                LOG_INFO("    e%: pos=(%, %), vel=(%, %)",
                         FMT_UINT(ecs_entity_index(it.entities[i])),
                         FMT_UINT((u32)p[i].x), FMT_UINT((u32)p[i].y),
                         FMT_UINT((u32)v[i].x), FMT_UINT((u32)v[i].y));
            }
            total += it.count;
        }
        LOG_INFO("Matched % entities (expected 5, excludes frozen)", FMT_UINT(total));
    }

    LOG_INFO("");
    LOG_INFO("=== Test 3: OPTIONAL query (Position, Velocity, ?Health) ===");
    {
        EcsTerm terms[] = {
            ecs_term(ecs_id(Position)),
            ecs_term(ecs_id(Velocity)),
            ecs_term_optional(ecs_id(Health)),
        };
        EcsQuery query;
        ecs_query_init_terms(&query, &g_world, terms, 3);

        LOG_INFO("Query has % terms, % fields", FMT_UINT(query.term_count), FMT_UINT(query.field_count));

        EcsIter it = ecs_query_iter(&query);
        i32 total = 0;
        i32 with_health = 0;
        while (ecs_iter_next(&it)) {
            Position *p = ecs_field(&it, Position, 0);
            Velocity *v = ecs_field(&it, Velocity, 1);
            Health *h = ecs_field(&it, Health, 2);

            LOG_INFO("  Table %: % entities, health_set=%",
                     FMT_UINT(it.table->id), FMT_UINT(it.count),
                     FMT_UINT(ecs_field_is_set(&it, 2)));

            for (i32 i = 0; i < it.count; i++) {
                if (ecs_field_is_set(&it, 2)) {
                    LOG_INFO("    e%: pos=(%, %), vel=(%, %), hp=%",
                             FMT_UINT(ecs_entity_index(it.entities[i])),
                             FMT_UINT((u32)p[i].x), FMT_UINT((u32)p[i].y),
                             FMT_UINT((u32)v[i].x), FMT_UINT((u32)v[i].y),
                             FMT_UINT((u32)h[i].value));
                    with_health++;
                } else {
                    LOG_INFO("    e%: pos=(%, %), vel=(%, %), hp=<none>",
                             FMT_UINT(ecs_entity_index(it.entities[i])),
                             FMT_UINT((u32)p[i].x), FMT_UINT((u32)p[i].y),
                             FMT_UINT((u32)v[i].x), FMT_UINT((u32)v[i].y));
                }
            }
            total += it.count;
        }
        LOG_INFO("Matched % entities total, % with health", FMT_UINT(total), FMT_UINT(with_health));
    }

    LOG_INFO("");
    LOG_INFO("=== Test 4: OR query (Position, Mana || Stamina) ===");
    {
        EcsTerm terms[] = {
            ecs_term(ecs_id(Position)),
            ecs_term_or(ecs_id(Mana), 2),
            ecs_term_or(ecs_id(Stamina), 0),
        };
        EcsQuery query;
        ecs_query_init_terms(&query, &g_world, terms, 3);

        LOG_INFO("Query has % terms, % fields", FMT_UINT(query.term_count), FMT_UINT(query.field_count));

        EcsIter it = ecs_query_iter(&query);
        i32 total = 0;
        while (ecs_iter_next(&it)) {
            Position *p = ecs_field(&it, Position, 0);
            LOG_INFO("  Table %: % entities", FMT_UINT(it.table->id), FMT_UINT(it.count));
            for (i32 i = 0; i < it.count; i++) {
                LOG_INFO("    e%: pos=(%, %)",
                         FMT_UINT(ecs_entity_index(it.entities[i])),
                         FMT_UINT((u32)p[i].x), FMT_UINT((u32)p[i].y));
            }
            total += it.count;
        }
        LOG_INFO("Matched % entities (expected 4: 2 with Mana + 2 with Stamina)", FMT_UINT(total));
    }

    LOG_INFO("");
    LOG_INFO("=== Test 5: Combined operators (Position, Velocity, !Frozen, ?Health) ===");
    {
        EcsTerm terms[] = {
            ecs_term(ecs_id(Position)),
            ecs_term(ecs_id(Velocity)),
            ecs_term_not(ecs_id(Frozen)),
            ecs_term_optional(ecs_id(Health)),
        };
        EcsQuery query;
        ecs_query_init_terms(&query, &g_world, terms, 4);

        LOG_INFO("Query has % terms, % fields", FMT_UINT(query.term_count), FMT_UINT(query.field_count));

        EcsIter it = ecs_query_iter(&query);
        i32 total = 0;
        while (ecs_iter_next(&it)) {
            Position *p = ecs_field(&it, Position, 0);
            Velocity *v = ecs_field(&it, Velocity, 1);
            Health *h = ecs_field(&it, Health, 2);

            LOG_INFO("  Table %: % entities", FMT_UINT(it.table->id), FMT_UINT(it.count));

            for (i32 i = 0; i < it.count; i++) {
                if (ecs_field_is_set(&it, 2)) {
                    LOG_INFO("    e%: pos=(%, %), hp=%",
                             FMT_UINT(ecs_entity_index(it.entities[i])),
                             FMT_UINT((u32)p[i].x), FMT_UINT((u32)p[i].y),
                             FMT_UINT((u32)h[i].value));
                } else {
                    LOG_INFO("    e%: pos=(%, %), hp=<none>",
                             FMT_UINT(ecs_entity_index(it.entities[i])),
                             FMT_UINT((u32)p[i].x), FMT_UINT((u32)p[i].y));
                }
            }
            total += it.count;
        }
        LOG_INFO("Matched % entities (expected 5: not frozen, some with health)", FMT_UINT(total));
    }

    LOG_INFO("");
    LOG_INFO("=== Component Record Stats ===");
    EcsComponentRecord *cr_pos = ecs_component_record_get(&g_world, ecs_id(Position));
    EcsComponentRecord *cr_vel = ecs_component_record_get(&g_world, ecs_id(Velocity));
    EcsComponentRecord *cr_hp = ecs_component_record_get(&g_world, ecs_id(Health));
    EcsComponentRecord *cr_frozen = ecs_component_record_get(&g_world, ecs_id(Frozen));

    LOG_INFO("Position: % tables", FMT_UINT(cr_pos ? cr_pos->table_count : 0));
    LOG_INFO("Velocity: % tables", FMT_UINT(cr_vel ? cr_vel->table_count : 0));
    LOG_INFO("Health: % tables", FMT_UINT(cr_hp ? cr_hp->table_count : 0));
    LOG_INFO("Frozen: % tables", FMT_UINT(cr_frozen ? cr_frozen->table_count : 0));

    LOG_INFO("");
    LOG_INFO("=== Query Term Operators Tests Complete ===");
}

void app_update_and_render(AppMemory *memory) {
    UNUSED(memory);
}
