void test_ecs(void) {
    if (!is_main_thread()) {
        return;
    }

    AppContext *app_ctx = app_ctx_current();

    EcsWorld world;
    ecs_world_init(&world, &app_ctx->arena);

    EcsEntity e1 = ecs_entity_new(&world);
    EcsEntity e2 = ecs_entity_new(&world);
    EcsEntity e3 = ecs_entity_new(&world);

    assert_eq(ecs_entity_index(e2), ecs_entity_index(e1) + 1);
    assert_eq(ecs_entity_index(e3), ecs_entity_index(e2) + 1);
    assert_eq(ecs_entity_generation(e1), 0);
    assert_eq(ecs_entity_generation(e2), 0);
    assert_eq(ecs_entity_generation(e3), 0);
    assert_eq(ecs_entity_count(&world), 3);

    assert_true(ecs_entity_is_alive(&world, e1));
    assert_true(ecs_entity_is_alive(&world, e2));
    assert_true(ecs_entity_is_alive(&world, e3));

    ecs_entity_delete(&world, e2);
    assert_eq(ecs_entity_count(&world), 2);

    assert_true(ecs_entity_is_alive(&world, e1));
    assert_false(ecs_entity_is_alive(&world, e2));
    assert_true(ecs_entity_is_alive(&world, e3));

    EcsEntity e4 = ecs_entity_new(&world);
    assert_eq(ecs_entity_index(e4), ecs_entity_index(e2));
    assert_eq(ecs_entity_generation(e4), 1);
    assert_eq(ecs_entity_count(&world), 3);

    assert_false(ecs_entity_is_alive(&world, e2));
    assert_true(ecs_entity_is_alive(&world, e4));

    for (u32 i = 0; i < 100; i++) {
        ecs_entity_new(&world);
    }
    assert_eq(ecs_entity_count(&world), 103);

    EcsEntity entities[50];
    for (u32 i = 0; i < 50; i++) {
        entities[i] = ecs_entity_new(&world);
    }
    assert_eq(ecs_entity_count(&world), 153);

    for (u32 i = 0; i < 50; i++) {
        ecs_entity_delete(&world, entities[i]);
    }
    assert_eq(ecs_entity_count(&world), 103);

    for (u32 i = 0; i < 50; i++) {
        EcsEntity e = ecs_entity_new(&world);
        assert_eq(ecs_entity_generation(e), 1);
    }
    assert_eq(ecs_entity_count(&world), 153);
}
