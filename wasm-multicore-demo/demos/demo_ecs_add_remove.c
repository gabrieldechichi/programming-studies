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

    LOG_INFO("=== Add/Remove/Set/Get API Test ===");

    ECS_COMPONENT(&g_world, Position);
    ECS_COMPONENT(&g_world, Velocity);
    ECS_COMPONENT(&g_world, Health);

    LOG_INFO("--- Create entity and add components one by one ---");
    EcsEntity e1 = ecs_entity_new(&g_world);
    LOG_INFO("Created entity e1: %", FMT_UINT(ecs_entity_index(e1)));

    LOG_INFO("e1 has Position: %", FMT_UINT(ecs_has(&g_world, e1, ecs_id(Position))));

    ecs_add(&g_world, e1, ecs_id(Position));
    LOG_INFO("Added Position to e1");
    LOG_INFO("e1 has Position: %", FMT_UINT(ecs_has(&g_world, e1, ecs_id(Position))));

    EcsRecord *rec = ecs_entity_get_record(&g_world, e1);
    LOG_INFO("e1 table id: %, type count: %",
             FMT_UINT(rec->table->id),
             FMT_UINT(rec->table->type.count));

    ecs_add(&g_world, e1, ecs_id(Velocity));
    LOG_INFO("Added Velocity to e1");
    rec = ecs_entity_get_record(&g_world, e1);
    LOG_INFO("e1 table id: %, type count: %",
             FMT_UINT(rec->table->id),
             FMT_UINT(rec->table->type.count));

    ecs_add(&g_world, e1, ecs_id(Health));
    LOG_INFO("Added Health to e1");
    rec = ecs_entity_get_record(&g_world, e1);
    LOG_INFO("e1 table id: %, type count: %",
             FMT_UINT(rec->table->id),
             FMT_UINT(rec->table->type.count));

    LOG_INFO("--- Use ecs_set to set component values ---");
    ecs_set(&g_world, e1, Position, { .x = 10.0f, .y = 20.0f });
    ecs_set(&g_world, e1, Velocity, { .x = 1.0f, .y = 2.0f });
    ecs_set(&g_world, e1, Health, { .value = 100.0f });

    LOG_INFO("--- Use ecs_get to read component values ---");
    Position *pos = ecs_get_component(&g_world, e1, Position);
    Velocity *vel = ecs_get_component(&g_world, e1, Velocity);
    Health *hp = ecs_get_component(&g_world, e1, Health);

    LOG_INFO("e1 Position: (%, %)", FMT_UINT((u32)pos->x), FMT_UINT((u32)pos->y));
    LOG_INFO("e1 Velocity: (%, %)", FMT_UINT((u32)vel->x), FMT_UINT((u32)vel->y));
    LOG_INFO("e1 Health: %", FMT_UINT((u32)hp->value));

    LOG_INFO("--- Remove Velocity component ---");
    ecs_remove(&g_world, e1, ecs_id(Velocity));
    LOG_INFO("e1 has Velocity: %", FMT_UINT(ecs_has(&g_world, e1, ecs_id(Velocity))));
    LOG_INFO("e1 has Position: %", FMT_UINT(ecs_has(&g_world, e1, ecs_id(Position))));
    LOG_INFO("e1 has Health: %", FMT_UINT(ecs_has(&g_world, e1, ecs_id(Health))));

    rec = ecs_entity_get_record(&g_world, e1);
    LOG_INFO("e1 table id: %, type count: %",
             FMT_UINT(rec->table->id),
             FMT_UINT(rec->table->type.count));

    LOG_INFO("--- Verify data preserved after remove ---");
    pos = ecs_get_component(&g_world, e1, Position);
    hp = ecs_get_component(&g_world, e1, Health);
    LOG_INFO("e1 Position: (%, %)", FMT_UINT((u32)pos->x), FMT_UINT((u32)pos->y));
    LOG_INFO("e1 Health: %", FMT_UINT((u32)hp->value));

    LOG_INFO("--- Test ecs_set on entity without component (auto-add) ---");
    EcsEntity e2 = ecs_entity_new(&g_world);
    LOG_INFO("Created entity e2: %", FMT_UINT(ecs_entity_index(e2)));
    LOG_INFO("e2 has Position: %", FMT_UINT(ecs_has(&g_world, e2, ecs_id(Position))));

    ecs_set(&g_world, e2, Position, { .x = 50.0f, .y = 60.0f });
    LOG_INFO("Called ecs_set for Position on e2");
    LOG_INFO("e2 has Position: %", FMT_UINT(ecs_has(&g_world, e2, ecs_id(Position))));

    pos = ecs_get_component(&g_world, e2, Position);
    LOG_INFO("e2 Position: (%, %)", FMT_UINT((u32)pos->x), FMT_UINT((u32)pos->y));

    LOG_INFO("--- Test graph edge caching ---");
    EcsEntity e3 = ecs_entity_new(&g_world);
    EcsEntity e4 = ecs_entity_new(&g_world);
    EcsEntity e5 = ecs_entity_new(&g_world);

    ecs_add(&g_world, e3, ecs_id(Position));
    ecs_add(&g_world, e4, ecs_id(Position));
    ecs_add(&g_world, e5, ecs_id(Position));

    EcsRecord *rec3 = ecs_entity_get_record(&g_world, e3);
    EcsRecord *rec4 = ecs_entity_get_record(&g_world, e4);
    EcsRecord *rec5 = ecs_entity_get_record(&g_world, e5);

    LOG_INFO("e3, e4, e5 all in same table: %",
             FMT_UINT(rec3->table == rec4->table && rec4->table == rec5->table));
    LOG_INFO("Table id: %", FMT_UINT(rec3->table->id));
    LOG_INFO("Table entity count: %", FMT_UINT(rec3->table->data.count));

    LOG_INFO("--- Multiple adds/removes ---");
    ecs_add(&g_world, e3, ecs_id(Velocity));
    ecs_add(&g_world, e3, ecs_id(Health));
    LOG_INFO("e3 has [Position, Velocity, Health]");

    ecs_remove(&g_world, e3, ecs_id(Position));
    LOG_INFO("Removed Position from e3");
    LOG_INFO("e3 has Position: %", FMT_UINT(ecs_has(&g_world, e3, ecs_id(Position))));
    LOG_INFO("e3 has Velocity: %", FMT_UINT(ecs_has(&g_world, e3, ecs_id(Velocity))));
    LOG_INFO("e3 has Health: %", FMT_UINT(ecs_has(&g_world, e3, ecs_id(Health))));

    LOG_INFO("--- Final table count ---");
    LOG_INFO("Total tables in world: %", FMT_UINT(g_world.store.table_count));

    LOG_INFO("=== Add/Remove/Set/Get Tests Complete ===");
}

void app_update_and_render(AppMemory *memory) {
    UNUSED(memory);
}
