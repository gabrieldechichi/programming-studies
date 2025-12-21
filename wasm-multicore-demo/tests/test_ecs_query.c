typedef struct { f32 x; f32 y; } QPosition;
typedef struct { f32 x; f32 y; } QVelocity;
typedef struct { f32 value; } QHealth;
typedef struct { f32 damage; } QAttack;
typedef struct { b32 frozen; } QFrozen;
typedef struct { f32 mana; } QMana;
typedef struct { f32 stamina; } QStamina;

void ecs_world_init_full_q(EcsWorld *world, ArenaAllocator *arena) {
    ecs_world_init(world, arena);
    ecs_store_init(world);
}

void test_ecs_query(void) {
    if (!is_main_thread()) {
        return;
    }

    ThreadContext *tctx = tctx_current();

    EcsWorld world;
    ecs_world_init_full_q(&world, &tctx->temp_arena);

    ECS_COMPONENT(&world, QPosition);
    ECS_COMPONENT(&world, QVelocity);
    ECS_COMPONENT(&world, QHealth);
    ECS_COMPONENT(&world, QAttack);
    ECS_COMPONENT(&world, QFrozen);
    ECS_COMPONENT(&world, QMana);
    ECS_COMPONENT(&world, QStamina);

    for (i32 i = 0; i < 3; i++) {
        EcsEntity e = ecs_entity_new(&world);
        ecs_set(&world, e, QPosition, { .x = (f32)(i * 10), .y = (f32)(i * 10) });
        ecs_set(&world, e, QVelocity, { .x = (f32)(i + 1), .y = (f32)(i + 1) });
    }

    for (i32 i = 0; i < 2; i++) {
        EcsEntity e = ecs_entity_new(&world);
        ecs_set(&world, e, QPosition, { .x = (f32)(100 + i * 10), .y = (f32)(100 + i * 10) });
        ecs_set(&world, e, QVelocity, { .x = (f32)(i + 1), .y = (f32)(i + 1) });
        ecs_set(&world, e, QHealth, { .value = (f32)(100 - i * 10) });
    }

    for (i32 i = 0; i < 2; i++) {
        EcsEntity e = ecs_entity_new(&world);
        ecs_set(&world, e, QPosition, { .x = (f32)(200 + i * 10), .y = (f32)(200 + i * 10) });
        ecs_set(&world, e, QVelocity, { .x = 0.0f, .y = 0.0f });
        ecs_set(&world, e, QFrozen, { .frozen = true });
    }

    for (i32 i = 0; i < 2; i++) {
        EcsEntity e = ecs_entity_new(&world);
        ecs_set(&world, e, QPosition, { .x = (f32)(300 + i * 10), .y = (f32)(300 + i * 10) });
        ecs_set(&world, e, QMana, { .mana = (f32)(50 + i * 25) });
    }

    for (i32 i = 0; i < 2; i++) {
        EcsEntity e = ecs_entity_new(&world);
        ecs_set(&world, e, QPosition, { .x = (f32)(400 + i * 10), .y = (f32)(400 + i * 10) });
        ecs_set(&world, e, QStamina, { .stamina = (f32)(100 + i * 10) });
    }

    assert_eq(ecs_entity_count(&world), 18);

    {
        EcsEntity terms[] = { ecs_id(QPosition), ecs_id(QVelocity) };
        EcsQuery query;
        ecs_query_init(&query, &world, terms, 2);

        EcsIter it = ecs_query_iter(&query);
        i32 total = 0;
        while (ecs_iter_next(&it)) {
            total += it.count;
        }
        assert_eq(total, 7);
    }

    {
        EcsTerm terms[] = {
            ecs_term(ecs_id(QPosition)),
            ecs_term(ecs_id(QVelocity)),
            ecs_term_not(ecs_id(QFrozen)),
        };
        EcsQuery query;
        ecs_query_init_terms(&query, &world, terms, 3);

        EcsIter it = ecs_query_iter(&query);
        i32 total = 0;
        while (ecs_iter_next(&it)) {
            total += it.count;
        }
        assert_eq(total, 5);
    }

    {
        EcsTerm terms[] = {
            ecs_term(ecs_id(QPosition)),
            ecs_term(ecs_id(QVelocity)),
            ecs_term_optional(ecs_id(QHealth)),
        };
        EcsQuery query;
        ecs_query_init_terms(&query, &world, terms, 3);

        EcsIter it = ecs_query_iter(&query);
        i32 total = 0;
        i32 with_health = 0;
        while (ecs_iter_next(&it)) {
            if (ecs_field_is_set(&it, 2)) {
                with_health += it.count;
            }
            total += it.count;
        }
        assert_eq(total, 7);
        assert_eq(with_health, 2);
    }

    {
        EcsTerm terms[] = {
            ecs_term(ecs_id(QPosition)),
            ecs_term_or(ecs_id(QMana), 2),
            ecs_term_or(ecs_id(QStamina), 0),
        };
        EcsQuery query;
        ecs_query_init_terms(&query, &world, terms, 3);

        EcsIter it = ecs_query_iter(&query);
        i32 total = 0;
        while (ecs_iter_next(&it)) {
            total += it.count;
        }
        assert_eq(total, 4);
    }

    {
        EcsTerm terms[] = {
            ecs_term(ecs_id(QPosition)),
            ecs_term(ecs_id(QVelocity)),
            ecs_term_not(ecs_id(QFrozen)),
            ecs_term_optional(ecs_id(QHealth)),
        };
        EcsQuery query;
        ecs_query_init_terms(&query, &world, terms, 4);

        EcsIter it = ecs_query_iter(&query);
        i32 total = 0;
        while (ecs_iter_next(&it)) {
            total += it.count;
        }
        assert_eq(total, 5);
    }

    EcsComponentRecord *cr_pos = ecs_component_record_get(&world, ecs_id(QPosition));
    EcsComponentRecord *cr_vel = ecs_component_record_get(&world, ecs_id(QVelocity));
    EcsComponentRecord *cr_hp = ecs_component_record_get(&world, ecs_id(QHealth));
    EcsComponentRecord *cr_frozen = ecs_component_record_get(&world, ecs_id(QFrozen));

    assert_true(cr_pos != NULL);
    assert_true(cr_vel != NULL);
    assert_true(cr_hp != NULL);
    assert_true(cr_frozen != NULL);
    assert_true(cr_pos->table_count >= 5);
    assert_true(cr_vel->table_count >= 3);
}
