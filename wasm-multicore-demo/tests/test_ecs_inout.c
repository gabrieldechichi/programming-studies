typedef struct { f32 x; f32 y; } IOPosition;
typedef struct { f32 x; f32 y; } IOVelocity;
typedef struct { f32 value; } IOHealth;
typedef struct { f32 radius; } IOCollider;

void ecs_world_init_full_io(EcsWorld *world, ArenaAllocator *arena) {
    ecs_world_init(world, arena);
    ecs_store_init(world);
}

void test_ecs_inout(void) {
    ThreadContext *tctx = tctx_current();

    EcsWorld world;
    ecs_world_init_full_io(&world, &tctx->temp_arena);

    ECS_COMPONENT(&world, IOPosition);
    ECS_COMPONENT(&world, IOVelocity);
    ECS_COMPONENT(&world, IOHealth);
    ECS_COMPONENT(&world, IOCollider);

    for (i32 i = 0; i < 5; i++) {
        EcsEntity e = ecs_entity_new(&world);
        ecs_set(&world, e, IOPosition, { .x = (f32)(i * 10), .y = (f32)(i * 10) });
        ecs_set(&world, e, IOVelocity, { .x = 1.0f, .y = 1.0f });
        ecs_set(&world, e, IOHealth, { .value = 100.0f });
        ecs_set(&world, e, IOCollider, { .radius = 5.0f });
    }

    {
        EcsTerm move_terms[] = {
            ecs_term_out(ecs_id(IOPosition)),
            ecs_term_in(ecs_id(IOVelocity)),
        };
        EcsQuery move_query;
        ecs_query_init_terms(&move_query, &world, move_terms, 2);

        assert_eq(move_query.read_fields, 0x2);
        assert_eq(move_query.write_fields, 0x1);
    }

    {
        EcsTerm render_terms[] = {
            ecs_term_in(ecs_id(IOPosition)),
            ecs_term_in(ecs_id(IOCollider)),
        };
        EcsQuery render_query;
        ecs_query_init_terms(&render_query, &world, render_terms, 2);

        assert_eq(render_query.read_fields, 0x3);
        assert_eq(render_query.write_fields, 0x0);
    }

    {
        EcsTerm collision_terms[] = {
            ecs_term_inout(ecs_id(IOPosition)),
            ecs_term_in(ecs_id(IOCollider)),
        };
        EcsQuery collision_query;
        ecs_query_init_terms(&collision_query, &world, collision_terms, 2);

        assert_eq(collision_query.read_fields, 0x3);
        assert_eq(collision_query.write_fields, 0x1);
    }

    {
        EcsTerm filter_terms[] = {
            ecs_term_in(ecs_id(IOPosition)),
            ecs_term_none(ecs_id(IOHealth)),
        };
        EcsQuery filter_query;
        ecs_query_init_terms(&filter_query, &world, filter_terms, 2);

        assert_eq(filter_query.field_count, 2);
    }

    {
        EcsTerm default_terms[] = {
            ecs_term(ecs_id(IOPosition)),
            ecs_term(ecs_id(IOVelocity)),
        };
        EcsQuery default_query;
        ecs_query_init_terms(&default_query, &world, default_terms, 2);

        assert_eq(default_query.read_fields, 0x3);
        assert_eq(default_query.write_fields, 0x3);
    }

    {
        EcsTerm mixed_terms[] = {
            ecs_term_out(ecs_id(IOPosition)),
            ecs_term_in(ecs_id(IOVelocity)),
            ecs_term_optional(ecs_id(IOHealth)),
        };
        EcsQuery mixed_query;
        ecs_query_init_terms(&mixed_query, &world, mixed_terms, 3);

        assert_eq(mixed_query.field_count, 3);
    }

    {
        EcsTerm move_terms[] = {
            ecs_term_out(ecs_id(IOPosition)),
            ecs_term_in(ecs_id(IOVelocity)),
        };
        EcsQuery move_query;
        ecs_query_init_terms(&move_query, &world, move_terms, 2);

        EcsIter it = ecs_query_iter(&move_query);
        i32 moved = 0;
        while (ecs_iter_next(&it)) {
            IOPosition *p = ecs_field(&it, IOPosition, 0);
            IOVelocity *v = ecs_field(&it, IOVelocity, 1);

            for (i32 i = 0; i < it.count; i++) {
                p[i].x += v[i].x;
                p[i].y += v[i].y;
            }
            moved += it.count;
        }
        assert_eq(moved, 5);
    }

    {
        EcsTerm render_terms[] = {
            ecs_term_in(ecs_id(IOPosition)),
        };
        EcsQuery render_query;
        ecs_query_init_terms(&render_query, &world, render_terms, 1);

        EcsIter verify_it = ecs_query_iter(&render_query);
        while (ecs_iter_next(&verify_it)) {
            IOPosition *p = ecs_field(&verify_it, IOPosition, 0);

            assert_eq((u32)p[0].x, 1);
            assert_eq((u32)p[1].x, 11);
        }
    }
}
