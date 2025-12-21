typedef struct { f32 x; f32 y; } QCPosition;
typedef struct { f32 x; f32 y; } QCVelocity;
typedef struct { f32 value; } QCHealth;

void ecs_world_init_full_qc(EcsWorld *world, ArenaAllocator *arena) {
    ecs_world_init(world, arena);
    ecs_store_init(world);
}

void test_ecs_query_cache(void) {
    if (!is_main_thread()) {
        return;
    }

    ThreadContext *tctx = tctx_current();

    EcsWorld world;
    ecs_world_init_full_qc(&world, &tctx->temp_arena);

    ECS_COMPONENT(&world, QCPosition);
    ECS_COMPONENT(&world, QCVelocity);
    ECS_COMPONENT(&world, QCHealth);

    for (i32 i = 0; i < 3; i++) {
        EcsEntity e = ecs_entity_new(&world);
        ecs_set(&world, e, QCPosition, { .x = (f32)(i * 10), .y = (f32)(i * 10) });
        ecs_set(&world, e, QCVelocity, { .x = 1.0f, .y = 1.0f });
    }

    EcsQuery move_query;
    EcsEntity terms[] = { ecs_id(QCPosition), ecs_id(QCVelocity) };
    ecs_query_init(&move_query, &world, terms, 2);
    ecs_query_cache_init(&move_query);

    assert_eq(move_query.cache.match_count, 1);
    assert_eq(world.cached_query_count, 1);

    EcsIter it = ecs_query_iter(&move_query);
    i32 total = 0;
    while (ecs_iter_next(&it)) {
        total += it.count;
    }
    assert_eq(total, 3);

    for (i32 i = 0; i < 4; i++) {
        EcsEntity e = ecs_entity_new(&world);
        ecs_set(&world, e, QCPosition, { .x = (f32)(100 + i * 10), .y = (f32)(100 + i * 10) });
        ecs_set(&world, e, QCVelocity, { .x = 2.0f, .y = 2.0f });
    }

    assert_eq(move_query.cache.match_count, 1);

    it = ecs_query_iter(&move_query);
    total = 0;
    while (ecs_iter_next(&it)) {
        total += it.count;
    }
    assert_eq(total, 7);

    for (i32 i = 0; i < 2; i++) {
        EcsEntity e = ecs_entity_new(&world);
        ecs_set(&world, e, QCPosition, { .x = (f32)(200 + i * 10), .y = (f32)(200 + i * 10) });
        ecs_set(&world, e, QCVelocity, { .x = 3.0f, .y = 3.0f });
        ecs_set(&world, e, QCHealth, { .value = 100.0f });
    }

    assert_eq(move_query.cache.match_count, 2);

    it = ecs_query_iter(&move_query);
    total = 0;
    while (ecs_iter_next(&it)) {
        total += it.count;
    }
    assert_eq(total, 9);

    for (i32 i = 0; i < 5; i++) {
        EcsEntity e = ecs_entity_new(&world);
        ecs_set(&world, e, QCPosition, { .x = 0.0f, .y = 0.0f });
    }

    assert_eq(move_query.cache.match_count, 2);
}
