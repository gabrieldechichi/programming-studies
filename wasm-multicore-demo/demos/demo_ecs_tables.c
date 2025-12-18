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
    LOG_INFO("ECS World initialized with tables");

    LOG_INFO("=== Table/Archetype Test ===");

    LOG_INFO("--- Register components ---");
    ECS_COMPONENT(&g_world, Position);
    ECS_COMPONENT(&g_world, Velocity);
    ECS_COMPONENT(&g_world, Health);

    LOG_INFO("Position id: %", FMT_UINT(ecs_entity_index(ecs_id(Position))));
    LOG_INFO("Velocity id: %", FMT_UINT(ecs_entity_index(ecs_id(Velocity))));
    LOG_INFO("Health id: %", FMT_UINT(ecs_entity_index(ecs_id(Health))));

    LOG_INFO("--- Test root table (empty archetype) ---");
    EcsTable *root = g_world.store.root;
    LOG_INFO("Root table id: %", FMT_UINT(root->id));
    LOG_INFO("Root table type count: %", FMT_UINT(root->type.count));
    LOG_INFO("Root table column count: %", FMT_UINT(root->column_count));

    LOG_INFO("--- Create archetype [Position, Velocity] ---");
    EcsEntity type_ids_1[] = { ecs_id(Position), ecs_id(Velocity) };
    EcsType type_1 = { .array = type_ids_1, .count = 2 };
    EcsTable *table_1 = ecs_table_find_or_create(&g_world, &type_1);

    LOG_INFO("Table 1 id: %", FMT_UINT(table_1->id));
    LOG_INFO("Table 1 type count: %", FMT_UINT(table_1->type.count));
    LOG_INFO("Table 1 column count: %", FMT_UINT(table_1->column_count));

    LOG_INFO("--- Create archetype [Position, Velocity, Health] ---");
    EcsEntity type_ids_2[] = { ecs_id(Position), ecs_id(Velocity), ecs_id(Health) };
    EcsType type_2 = { .array = type_ids_2, .count = 3 };
    EcsTable *table_2 = ecs_table_find_or_create(&g_world, &type_2);

    LOG_INFO("Table 2 id: %", FMT_UINT(table_2->id));
    LOG_INFO("Table 2 type count: %", FMT_UINT(table_2->type.count));
    LOG_INFO("Table 2 column count: %", FMT_UINT(table_2->column_count));

    LOG_INFO("--- Find existing table (should return same) ---");
    EcsTable *table_1_again = ecs_table_find_or_create(&g_world, &type_1);
    LOG_INFO("Table 1 found again, id: %", FMT_UINT(table_1_again->id));
    LOG_INFO("Same table: %", FMT_UINT(table_1 == table_1_again));

    LOG_INFO("--- Add entities to table_1 [Position, Velocity] ---");
    EcsEntity e1 = ecs_entity_new(&g_world);
    EcsEntity e2 = ecs_entity_new(&g_world);
    EcsEntity e3 = ecs_entity_new(&g_world);

    i32 row1 = ecs_table_append(&g_world, table_1, e1);
    i32 row2 = ecs_table_append(&g_world, table_1, e2);
    i32 row3 = ecs_table_append(&g_world, table_1, e3);

    LOG_INFO("Entity e1 row: %", FMT_UINT(row1));
    LOG_INFO("Entity e2 row: %", FMT_UINT(row2));
    LOG_INFO("Entity e3 row: %", FMT_UINT(row3));
    LOG_INFO("Table 1 count: %", FMT_UINT(table_1->data.count));

    LOG_INFO("--- Set component data ---");
    i32 pos_col_idx;
    Position *positions = (Position*)ecs_table_get_column(table_1, ecs_id(Position), &pos_col_idx);
    LOG_INFO("Position column index: %", FMT_UINT(pos_col_idx));

    i32 vel_col_idx;
    Velocity *velocities = (Velocity*)ecs_table_get_column(table_1, ecs_id(Velocity), &vel_col_idx);
    LOG_INFO("Velocity column index: %", FMT_UINT(vel_col_idx));

    positions[0] = (Position){ 10.0f, 20.0f };
    positions[1] = (Position){ 30.0f, 40.0f };
    positions[2] = (Position){ 50.0f, 60.0f };

    velocities[0] = (Velocity){ 1.0f, 2.0f };
    velocities[1] = (Velocity){ 3.0f, 4.0f };
    velocities[2] = (Velocity){ 5.0f, 6.0f };

    LOG_INFO("--- Read component data back ---");
    for (i32 i = 0; i < table_1->data.count; i++) {
        Position *p = (Position*)ecs_table_get_component(table_1, i, pos_col_idx);
        Velocity *v = (Velocity*)ecs_table_get_component(table_1, i, vel_col_idx);
        LOG_INFO("Row %: pos=(%, %), vel=(%, %)",
                 FMT_UINT(i),
                 FMT_UINT((u32)p->x), FMT_UINT((u32)p->y),
                 FMT_UINT((u32)v->x), FMT_UINT((u32)v->y));
    }

    LOG_INFO("--- Delete middle entity (e2) ---");
    ecs_table_delete(&g_world, table_1, row2);
    LOG_INFO("Table 1 count after delete: %", FMT_UINT(table_1->data.count));

    LOG_INFO("--- Read data after delete (e3 should have moved to row 1) ---");
    for (i32 i = 0; i < table_1->data.count; i++) {
        Position *p = (Position*)ecs_table_get_component(table_1, i, pos_col_idx);
        Velocity *v = (Velocity*)ecs_table_get_component(table_1, i, vel_col_idx);
        EcsEntity ent = table_1->data.entities[i];
        LOG_INFO("Row %: entity=%, pos=(%, %), vel=(%, %)",
                 FMT_UINT(i),
                 FMT_UINT(ecs_entity_index(ent)),
                 FMT_UINT((u32)p->x), FMT_UINT((u32)p->y),
                 FMT_UINT((u32)v->x), FMT_UINT((u32)v->y));
    }

    LOG_INFO("--- Verify entity record updated ---");
    EcsRecord *rec_e3 = ecs_entity_get_record(&g_world, e3);
    LOG_INFO("e3 record table id: %", FMT_UINT(rec_e3->table->id));
    LOG_INFO("e3 record row: %", FMT_UINT(rec_e3->row));

    LOG_INFO("--- Table count in world ---");
    LOG_INFO("Total tables: %", FMT_UINT(g_world.store.table_count));

    LOG_INFO("=== Table/Archetype Tests Complete ===");
}

void app_update_and_render(AppMemory *memory) {
    UNUSED(memory);
}
