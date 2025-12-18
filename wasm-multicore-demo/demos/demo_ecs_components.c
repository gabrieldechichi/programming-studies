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
    f32 m[16];
} Transform;

global EcsWorld g_world;

void app_init(AppMemory *memory) {
    UNUSED(memory);

    if (!is_main_thread()) {
        return;
    }

    AppContext *app_ctx = app_ctx_current();

    ecs_world_init(&g_world, &app_ctx->arena);
    LOG_INFO("ECS World initialized");

    LOG_INFO("=== Component Registration Test ===");

    LOG_INFO("--- Register components ---");
    ECS_COMPONENT(&g_world, Position);
    ECS_COMPONENT(&g_world, Velocity);
    ECS_COMPONENT(&g_world, Health);
    ECS_COMPONENT(&g_world, Transform);

    LOG_INFO("Position: id=%, size=%, align=%",
             FMT_UINT(ecs_entity_index(ecs_id(Position))),
             FMT_UINT(ecs_type_info_get(&g_world, ecs_id(Position))->size),
             FMT_UINT(ecs_type_info_get(&g_world, ecs_id(Position))->alignment));

    LOG_INFO("Velocity: id=%, size=%, align=%",
             FMT_UINT(ecs_entity_index(ecs_id(Velocity))),
             FMT_UINT(ecs_type_info_get(&g_world, ecs_id(Velocity))->size),
             FMT_UINT(ecs_type_info_get(&g_world, ecs_id(Velocity))->alignment));

    LOG_INFO("Health: id=%, size=%, align=%",
             FMT_UINT(ecs_entity_index(ecs_id(Health))),
             FMT_UINT(ecs_type_info_get(&g_world, ecs_id(Health))->size),
             FMT_UINT(ecs_type_info_get(&g_world, ecs_id(Health))->alignment));

    LOG_INFO("Transform: id=%, size=%, align=%",
             FMT_UINT(ecs_entity_index(ecs_id(Transform))),
             FMT_UINT(ecs_type_info_get(&g_world, ecs_id(Transform))->size),
             FMT_UINT(ecs_type_info_get(&g_world, ecs_id(Transform))->alignment));

    LOG_INFO("--- Verify component IDs are low (< 256) ---");
    LOG_INFO("All component IDs should be between 8 and 255");
    LOG_INFO("Position id=% < 256: %",
             FMT_UINT(ecs_entity_index(ecs_id(Position))),
             FMT_UINT(ecs_entity_index(ecs_id(Position)) < ECS_HI_COMPONENT_ID));

    LOG_INFO("--- Verify regular entities get high IDs (>= 384) ---");
    EcsEntity e1 = ecs_entity_new(&g_world);
    EcsEntity e2 = ecs_entity_new(&g_world);
    EcsEntity e3 = ecs_entity_new(&g_world);

    LOG_INFO("Entity 1: id=%", FMT_UINT(ecs_entity_index(e1)));
    LOG_INFO("Entity 2: id=%", FMT_UINT(ecs_entity_index(e2)));
    LOG_INFO("Entity 3: id=%", FMT_UINT(ecs_entity_index(e3)));

    LOG_INFO("e1 id=% >= 384: %",
             FMT_UINT(ecs_entity_index(e1)),
             FMT_UINT(ecs_entity_index(e1) >= ECS_FIRST_USER_ENTITY_ID));

    LOG_INFO("--- Verify no ID collision ---");
    LOG_INFO("Component count: %", FMT_UINT(g_world.type_info_count));
    LOG_INFO("Entity count: %", FMT_UINT(ecs_entity_count(&g_world)));

    LOG_INFO("Position is alive: %", FMT_UINT(ecs_entity_is_alive(&g_world, ecs_id(Position))));
    LOG_INFO("e1 is alive: %", FMT_UINT(ecs_entity_is_alive(&g_world, e1)));

    LOG_INFO("--- Register many components ---");
    for (u32 i = 0; i < 50; i++) {
        ecs_entity_new_low_id(&g_world);
    }
    LOG_INFO("Registered 50 more low-id entities");
    LOG_INFO("last_component_id now: %", FMT_UINT(g_world.last_component_id));

    LOG_INFO("--- Create many entities ---");
    for (u32 i = 0; i < 100; i++) {
        ecs_entity_new(&g_world);
    }
    LOG_INFO("Created 100 entities");
    LOG_INFO("Entity count: %", FMT_UINT(ecs_entity_count(&g_world)));

    LOG_INFO("=== Component Registration Tests Complete ===");
}

void app_update_and_render(AppMemory *memory) {
    UNUSED(memory);
}
