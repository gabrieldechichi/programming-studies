typedef struct {
    f32 x;
    f32 y;
} TestPosition;

typedef struct {
    f32 x;
    f32 y;
} TestVelocity;

typedef struct {
    f32 value;
} TestHealth;

typedef struct {
    f32 m[16];
} TestTransform;

void test_ecs_components(void) {
    ThreadContext *tctx = tctx_current();

    EcsWorld world;
    ecs_world_init(&world, &tctx->temp_arena);

    ECS_COMPONENT(&world, TestPosition);
    ECS_COMPONENT(&world, TestVelocity);
    ECS_COMPONENT(&world, TestHealth);
    ECS_COMPONENT(&world, TestTransform);

    const EcsTypeInfo *ti_pos = ecs_type_info_get(&world, ecs_id(TestPosition));
    const EcsTypeInfo *ti_vel = ecs_type_info_get(&world, ecs_id(TestVelocity));
    const EcsTypeInfo *ti_hp = ecs_type_info_get(&world, ecs_id(TestHealth));
    const EcsTypeInfo *ti_tr = ecs_type_info_get(&world, ecs_id(TestTransform));

    assert_eq(ti_pos->size, sizeof(TestPosition));
    assert_eq(ti_vel->size, sizeof(TestVelocity));
    assert_eq(ti_hp->size, sizeof(TestHealth));
    assert_eq(ti_tr->size, sizeof(TestTransform));

    assert_true(ecs_entity_index(ecs_id(TestPosition)) < ECS_HI_COMPONENT_ID);
    assert_true(ecs_entity_index(ecs_id(TestVelocity)) < ECS_HI_COMPONENT_ID);
    assert_true(ecs_entity_index(ecs_id(TestHealth)) < ECS_HI_COMPONENT_ID);
    assert_true(ecs_entity_index(ecs_id(TestTransform)) < ECS_HI_COMPONENT_ID);

    EcsEntity e1 = ecs_entity_new(&world);
    EcsEntity e2 = ecs_entity_new(&world);
    EcsEntity e3 = ecs_entity_new(&world);

    assert_true(ecs_entity_index(e1) >= ECS_FIRST_USER_ENTITY_ID);
    assert_true(ecs_entity_index(e2) >= ECS_FIRST_USER_ENTITY_ID);
    assert_true(ecs_entity_index(e3) >= ECS_FIRST_USER_ENTITY_ID);

    assert_eq(world.type_info_count, 4);
    assert_eq(ecs_entity_count(&world), 7);

    assert_true(ecs_entity_is_alive(&world, ecs_id(TestPosition)));
    assert_true(ecs_entity_is_alive(&world, e1));

    for (u32 i = 0; i < 50; i++) {
        ecs_entity_new_low_id(&world);
    }
    assert_true(world.last_component_id > 50);

    for (u32 i = 0; i < 100; i++) {
        ecs_entity_new(&world);
    }
    assert_eq(ecs_entity_count(&world), 157);
}
