typedef struct { f32 x; f32 y; } CDPosition;
typedef struct { f32 x; f32 y; } CDVelocity;
typedef struct { f32 value; } CDHealth;

void ecs_world_init_full_cd(EcsWorld *world, ArenaAllocator *arena) {
    ecs_world_init(world, arena);
    ecs_store_init(world);
}

void test_ecs_change_detection(void) {
    if (!is_main_thread()) {
        return;
    }

    AppContext *app_ctx = app_ctx_current();

    EcsWorld world;
    ecs_world_init_full_cd(&world, &app_ctx->arena);

    EcsEntity comp_position = ecs_component_register(&world, sizeof(CDPosition), _Alignof(CDPosition), "CDPosition");
    EcsEntity comp_velocity = ecs_component_register(&world, sizeof(CDVelocity), _Alignof(CDVelocity), "CDVelocity");

    EcsEntity test_entity = 0;
    for (i32 i = 0; i < 3; i++) {
        EcsEntity e = ecs_entity_new(&world);
        CDPosition p = { (f32)(i * 10), (f32)(i * 10) };
        CDVelocity v = { 1.0f, 1.0f };
        ecs_set_ptr(&world, e, comp_position, &p);
        ecs_set_ptr(&world, e, comp_velocity, &v);
    }
    test_entity = ecs_entity_new(&world);
    {
        CDPosition p = { 100.0f, 100.0f };
        CDVelocity v = { 2.0f, 2.0f };
        ecs_set_ptr(&world, test_entity, comp_position, &p);
        ecs_set_ptr(&world, test_entity, comp_velocity, &v);
    }

    EcsQuery move_query;
    EcsTerm move_terms[] = {
        ecs_term_inout(comp_position),
        ecs_term_in(comp_velocity),
    };
    ecs_query_init_terms(&move_query, &world, move_terms, 2);
    ecs_query_cache_init(&move_query);

    EcsQuery render_query;
    EcsTerm render_terms[] = {
        ecs_term_in(comp_position),
    };
    ecs_query_init_terms(&render_query, &world, render_terms, 1);
    ecs_query_cache_init(&render_query);

    ecs_query_sync(&move_query);
    ecs_query_sync(&render_query);

    assert_false(ecs_query_changed(&move_query));
    assert_false(ecs_query_changed(&render_query));

    {
        CDPosition p = { 200.0f, 200.0f };
        ecs_set_ptr(&world, test_entity, comp_position, &p);
    }

    assert_true(ecs_query_changed(&move_query));
    assert_true(ecs_query_changed(&render_query));

    ecs_query_sync(&move_query);
    ecs_query_sync(&render_query);

    {
        CDVelocity v = { 5.0f, 5.0f };
        ecs_set_ptr(&world, test_entity, comp_velocity, &v);
    }

    assert_true(ecs_query_changed(&move_query));
    assert_false(ecs_query_changed(&render_query));

    ecs_query_sync(&move_query);

    EcsEntity new_e = ecs_entity_new(&world);
    {
        CDPosition p = { 0.0f, 0.0f };
        CDVelocity v = { 1.0f, 1.0f };
        ecs_set_ptr(&world, new_e, comp_position, &p);
        ecs_set_ptr(&world, new_e, comp_velocity, &v);
    }

    assert_true(ecs_query_changed(&move_query));

    ecs_query_sync(&move_query);

    EcsIter it = ecs_query_iter(&move_query);
    while (ecs_iter_next(&it)) {
        b32 changed = ecs_iter_changed(&it);
        assert_false(changed);
    }

    {
        CDPosition p = { 300.0f, 300.0f };
        ecs_set_ptr(&world, test_entity, comp_position, &p);
    }

    it = ecs_query_iter(&move_query);
    i32 changed_count = 0;
    while (ecs_iter_next(&it)) {
        if (ecs_iter_changed(&it)) {
            changed_count++;
            ecs_iter_sync(&it);
        }
    }
    assert_true(changed_count > 0);

    it = ecs_query_iter(&move_query);
    while (ecs_iter_next(&it)) {
        assert_false(ecs_iter_changed(&it));
    }
}
