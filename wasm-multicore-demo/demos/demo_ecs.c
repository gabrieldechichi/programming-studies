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

global EcsWorld g_world;

void app_init(AppMemory *memory) {
    UNUSED(memory);

    if (!is_main_thread()) {
        return;
    }

    AppContext *app_ctx = app_ctx_current();

    ecs_world_init(&g_world, &app_ctx->arena);
    LOG_INFO("ECS World initialized");

    LOG_INFO("--- Test 1: Create entities ---");
    EcsEntity e1 = ecs_entity_new(&g_world);
    EcsEntity e2 = ecs_entity_new(&g_world);
    EcsEntity e3 = ecs_entity_new(&g_world);

    LOG_INFO("Created e1: index=%, gen=%",
             FMT_UINT(ecs_entity_index(e1)),
             FMT_UINT(ecs_entity_generation(e1)));
    LOG_INFO("Created e2: index=%, gen=%",
             FMT_UINT(ecs_entity_index(e2)),
             FMT_UINT(ecs_entity_generation(e2)));
    LOG_INFO("Created e3: index=%, gen=%",
             FMT_UINT(ecs_entity_index(e3)),
             FMT_UINT(ecs_entity_generation(e3)));

    LOG_INFO("Entity count: %", FMT_UINT(ecs_entity_count(&g_world)));

    LOG_INFO("--- Test 2: Check alive status ---");
    LOG_INFO("e1 alive: %", FMT_UINT(ecs_entity_is_alive(&g_world, e1)));
    LOG_INFO("e2 alive: %", FMT_UINT(ecs_entity_is_alive(&g_world, e2)));
    LOG_INFO("e3 alive: %", FMT_UINT(ecs_entity_is_alive(&g_world, e3)));

    LOG_INFO("--- Test 3: Delete e2 ---");
    ecs_entity_delete(&g_world, e2);
    LOG_INFO("Deleted e2");
    LOG_INFO("Entity count after delete: %", FMT_UINT(ecs_entity_count(&g_world)));

    LOG_INFO("e1 alive: %", FMT_UINT(ecs_entity_is_alive(&g_world, e1)));
    LOG_INFO("e2 alive: %", FMT_UINT(ecs_entity_is_alive(&g_world, e2)));
    LOG_INFO("e3 alive: %", FMT_UINT(ecs_entity_is_alive(&g_world, e3)));

    LOG_INFO("--- Test 4: Create new entity (should recycle e2's index) ---");
    EcsEntity e4 = ecs_entity_new(&g_world);
    LOG_INFO("Created e4: index=%, gen=%",
             FMT_UINT(ecs_entity_index(e4)),
             FMT_UINT(ecs_entity_generation(e4)));

    LOG_INFO("Entity count: %", FMT_UINT(ecs_entity_count(&g_world)));

    LOG_INFO("--- Test 5: Verify stale reference detection ---");
    LOG_INFO("e2 (stale) index=%, gen=%",
             FMT_UINT(ecs_entity_index(e2)),
             FMT_UINT(ecs_entity_generation(e2)));
    LOG_INFO("e4 (new)   index=%, gen=%",
             FMT_UINT(ecs_entity_index(e4)),
             FMT_UINT(ecs_entity_generation(e4)));
    LOG_INFO("e2 (stale) alive: %", FMT_UINT(ecs_entity_is_alive(&g_world, e2)));
    LOG_INFO("e4 (new) alive: %", FMT_UINT(ecs_entity_is_alive(&g_world, e4)));

    LOG_INFO("--- Test 6: Bulk create ---");
    for (u32 i = 0; i < 100; i++) {
        ecs_entity_new(&g_world);
    }
    LOG_INFO("Created 100 more entities");
    LOG_INFO("Total entity count: %", FMT_UINT(ecs_entity_count(&g_world)));

    LOG_INFO("--- Test 7: Bulk delete and recreate ---");
    EcsEntity entities[50];
    for (u32 i = 0; i < 50; i++) {
        entities[i] = ecs_entity_new(&g_world);
    }
    LOG_INFO("Created 50 entities");
    LOG_INFO("Entity count: %", FMT_UINT(ecs_entity_count(&g_world)));

    for (u32 i = 0; i < 50; i++) {
        ecs_entity_delete(&g_world, entities[i]);
    }
    LOG_INFO("Deleted 50 entities");
    LOG_INFO("Entity count: %", FMT_UINT(ecs_entity_count(&g_world)));

    for (u32 i = 0; i < 50; i++) {
        EcsEntity e = ecs_entity_new(&g_world);
        LOG_INFO("Recycled: index=%, gen=%",
                 FMT_UINT(ecs_entity_index(e)),
                 FMT_UINT(ecs_entity_generation(e)));
    }
    LOG_INFO("Recreated 50 entities (should all have gen=1)");
    LOG_INFO("Entity count: %", FMT_UINT(ecs_entity_count(&g_world)));

    LOG_INFO("=== ECS Entity Tests Complete ===");
}

void app_update_and_render(AppMemory *memory) {
    UNUSED(memory);
}
