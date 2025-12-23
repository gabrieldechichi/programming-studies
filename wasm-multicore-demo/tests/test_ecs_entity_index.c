typedef struct { f32 x; f32 y; } IdxPosition;
typedef struct { f32 x; f32 y; } IdxVelocity;
typedef struct { f32 value; } IdxHealth;

typedef struct {
    u32 *seen_indices;
    i32 max_index;
} EntityIndexTestCtx;

global EcsWorld g_idx_world;
global EntityIndexTestCtx g_idx_ctx;
global EcsEntity g_idx_position_id;
global EcsEntity g_idx_velocity_id;
global EcsEntity g_idx_health_id;

void ecs_world_init_full_idx(EcsWorld *world, ArenaAllocator *arena) {
    ecs_world_init(world, arena);
    ecs_store_init(world);
}

void IdxRecordSystem(EcsIter *it) {
    EntityIndexTestCtx *ctx = (EntityIndexTestCtx *)it->ctx;

    for (i32 i = 0; i < it->count; i++) {
        i32 entity_index = it->frame_offset + it->offset + i;
        debug_assert(entity_index >= 0 && entity_index < ctx->max_index);
        ins_atomic_u32_inc_eval(&ctx->seen_indices[entity_index]);
    }
}

void test_ecs_entity_index_single(void) {
    ThreadContext *tctx = tctx_current();

    if (is_main_thread()) {
        ecs_world_init_full_idx(&g_idx_world, &tctx->temp_arena);

        g_idx_position_id = ecs_component_register(&g_idx_world, sizeof(IdxPosition), _Alignof(IdxPosition), "IdxPosition");
        g_idx_velocity_id = ecs_component_register(&g_idx_world, sizeof(IdxVelocity), _Alignof(IdxVelocity), "IdxVelocity");
        g_idx_health_id = ecs_component_register(&g_idx_world, sizeof(IdxHealth), _Alignof(IdxHealth), "IdxHealth");

        for (i32 i = 0; i < 50; i++) {
            EcsEntity e = ecs_entity_new(&g_idx_world);
            ecs_add(&g_idx_world, e, g_idx_position_id);
            ecs_add(&g_idx_world, e, g_idx_velocity_id);
        }

        for (i32 i = 0; i < 30; i++) {
            EcsEntity e = ecs_entity_new(&g_idx_world);
            ecs_add(&g_idx_world, e, g_idx_position_id);
            ecs_add(&g_idx_world, e, g_idx_velocity_id);
            ecs_add(&g_idx_world, e, g_idx_health_id);
        }

        i32 total_query_entities = 80;

        g_idx_ctx.max_index = total_query_entities;
        g_idx_ctx.seen_indices = ARENA_ALLOC_ARRAY(&tctx->temp_arena, u32, total_query_entities);
        memset(g_idx_ctx.seen_indices, 0, sizeof(u32) * total_query_entities);

        EcsTerm terms[] = {
            ecs_term_in(g_idx_position_id),
            ecs_term_in(g_idx_velocity_id),
        };

        EcsSystemDesc desc = {0};
        desc.terms = terms;
        desc.term_count = 2;
        desc.callback = IdxRecordSystem;
        desc.ctx = &g_idx_ctx;
        desc.name = "IdxRecordSystemSingle";
        desc.thread_mode = ECS_THREAD_SINGLE;

        ecs_system_init(&g_idx_world, &desc);
    }

    lane_sync();

    ecs_progress(&g_idx_world, 0.016f);

    lane_sync();

    if (is_main_thread()) {
        for (i32 i = 0; i < g_idx_ctx.max_index; i++) {
            assert_eq(g_idx_ctx.seen_indices[i], 1);
        }
    }
}

void test_ecs_entity_index_multi(void) {
    ThreadContext *tctx = tctx_current();

    if (is_main_thread()) {
        ecs_world_init_full_idx(&g_idx_world, &tctx->temp_arena);

        g_idx_position_id = ecs_component_register(&g_idx_world, sizeof(IdxPosition), _Alignof(IdxPosition), "IdxPosition");
        g_idx_velocity_id = ecs_component_register(&g_idx_world, sizeof(IdxVelocity), _Alignof(IdxVelocity), "IdxVelocity");
        g_idx_health_id = ecs_component_register(&g_idx_world, sizeof(IdxHealth), _Alignof(IdxHealth), "IdxHealth");

        for (i32 i = 0; i < 100; i++) {
            EcsEntity e = ecs_entity_new(&g_idx_world);
            ecs_add(&g_idx_world, e, g_idx_position_id);
            ecs_add(&g_idx_world, e, g_idx_velocity_id);
        }

        for (i32 i = 0; i < 100; i++) {
            EcsEntity e = ecs_entity_new(&g_idx_world);
            ecs_add(&g_idx_world, e, g_idx_position_id);
            ecs_add(&g_idx_world, e, g_idx_velocity_id);
            ecs_add(&g_idx_world, e, g_idx_health_id);
        }

        for (i32 i = 0; i < 100; i++) {
            EcsEntity e = ecs_entity_new(&g_idx_world);
            ecs_add(&g_idx_world, e, g_idx_position_id);
        }

        i32 total_query_entities = 200;

        g_idx_ctx.max_index = total_query_entities;
        g_idx_ctx.seen_indices = ARENA_ALLOC_ARRAY(&tctx->temp_arena, u32, total_query_entities);
        memset(g_idx_ctx.seen_indices, 0, sizeof(u32) * total_query_entities);

        EcsTerm terms[] = {
            ecs_term_in(g_idx_position_id),
            ecs_term_in(g_idx_velocity_id),
        };

        EcsSystemDesc desc = {0};
        desc.terms = terms;
        desc.term_count = 2;
        desc.callback = IdxRecordSystem;
        desc.ctx = &g_idx_ctx;
        desc.name = "IdxRecordSystemMulti";
        desc.thread_mode = ECS_THREAD_MULTI;

        ecs_system_init(&g_idx_world, &desc);
    }

    lane_sync();

    ecs_progress(&g_idx_world, 0.016f);

    lane_sync();

    if (is_main_thread()) {
        for (i32 i = 0; i < g_idx_ctx.max_index; i++) {
            assert_eq(g_idx_ctx.seen_indices[i], 1);
        }
    }
}
