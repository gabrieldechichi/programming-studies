typedef struct { f32 x; f32 y; } ARPosition;
typedef struct { f32 x; f32 y; } ARVelocity;
typedef struct { f32 value; } ARHealth;

void ecs_world_init_full_ar(EcsWorld *world, ArenaAllocator *arena) {
    ecs_world_init(world, arena);
    ecs_store_init(world);
}

void test_ecs_add_remove(void) {
    if (!is_main_thread()) {
        return;
    }

    ThreadContext *tctx = tctx_current();

    EcsWorld world;
    ecs_world_init_full_ar(&world, &tctx->temp_arena);

    ECS_COMPONENT(&world, ARPosition);
    ECS_COMPONENT(&world, ARVelocity);
    ECS_COMPONENT(&world, ARHealth);

    EcsEntity e1 = ecs_entity_new(&world);
    assert_false(ecs_has(&world, e1, ecs_id(ARPosition)));

    ecs_add(&world, e1, ecs_id(ARPosition));
    assert_true(ecs_has(&world, e1, ecs_id(ARPosition)));

    EcsRecord *rec = ecs_entity_get_record(&world, e1);
    assert_eq(rec->table->type.count, 1);

    ecs_add(&world, e1, ecs_id(ARVelocity));
    assert_true(ecs_has(&world, e1, ecs_id(ARVelocity)));
    rec = ecs_entity_get_record(&world, e1);
    assert_eq(rec->table->type.count, 2);

    ecs_add(&world, e1, ecs_id(ARHealth));
    assert_true(ecs_has(&world, e1, ecs_id(ARHealth)));
    rec = ecs_entity_get_record(&world, e1);
    assert_eq(rec->table->type.count, 3);

    ecs_set(&world, e1, ARPosition, { .x = 10.0f, .y = 20.0f });
    ecs_set(&world, e1, ARVelocity, { .x = 1.0f, .y = 2.0f });
    ecs_set(&world, e1, ARHealth, { .value = 100.0f });

    ARPosition *pos = ecs_get_component(&world, e1, ARPosition);
    ARVelocity *vel = ecs_get_component(&world, e1, ARVelocity);
    ARHealth *hp = ecs_get_component(&world, e1, ARHealth);

    assert_eq((u32)pos->x, 10);
    assert_eq((u32)pos->y, 20);
    assert_eq((u32)vel->x, 1);
    assert_eq((u32)vel->y, 2);
    assert_eq((u32)hp->value, 100);

    ecs_remove(&world, e1, ecs_id(ARVelocity));
    assert_false(ecs_has(&world, e1, ecs_id(ARVelocity)));
    assert_true(ecs_has(&world, e1, ecs_id(ARPosition)));
    assert_true(ecs_has(&world, e1, ecs_id(ARHealth)));

    rec = ecs_entity_get_record(&world, e1);
    assert_eq(rec->table->type.count, 2);

    pos = ecs_get_component(&world, e1, ARPosition);
    hp = ecs_get_component(&world, e1, ARHealth);
    assert_eq((u32)pos->x, 10);
    assert_eq((u32)hp->value, 100);

    EcsEntity e2 = ecs_entity_new(&world);
    assert_false(ecs_has(&world, e2, ecs_id(ARPosition)));

    ecs_set(&world, e2, ARPosition, { .x = 50.0f, .y = 60.0f });
    assert_true(ecs_has(&world, e2, ecs_id(ARPosition)));

    pos = ecs_get_component(&world, e2, ARPosition);
    assert_eq((u32)pos->x, 50);

    EcsEntity e3 = ecs_entity_new(&world);
    EcsEntity e4 = ecs_entity_new(&world);
    EcsEntity e5 = ecs_entity_new(&world);

    ecs_add(&world, e3, ecs_id(ARPosition));
    ecs_add(&world, e4, ecs_id(ARPosition));
    ecs_add(&world, e5, ecs_id(ARPosition));

    EcsRecord *rec3 = ecs_entity_get_record(&world, e3);
    EcsRecord *rec4 = ecs_entity_get_record(&world, e4);
    EcsRecord *rec5 = ecs_entity_get_record(&world, e5);

    assert_true(rec3->table == rec4->table);
    assert_true(rec4->table == rec5->table);
    assert_eq(rec3->table->data.count, 4);

    ecs_add(&world, e3, ecs_id(ARVelocity));
    ecs_add(&world, e3, ecs_id(ARHealth));

    ecs_remove(&world, e3, ecs_id(ARPosition));
    assert_false(ecs_has(&world, e3, ecs_id(ARPosition)));
    assert_true(ecs_has(&world, e3, ecs_id(ARVelocity)));
    assert_true(ecs_has(&world, e3, ecs_id(ARHealth)));

    assert_true(world.store.table_count >= 4);
}
