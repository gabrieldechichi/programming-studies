typedef struct { f32 x; f32 y; } TblPosition;
typedef struct { f32 x; f32 y; } TblVelocity;
typedef struct { f32 value; } TblHealth;

void ecs_world_init_full_tbl(EcsWorld *world, ArenaAllocator *arena) {
    ecs_world_init(world, arena);
    ecs_store_init(world);
}

void test_ecs_tables(void) {
    if (!is_main_thread()) {
        return;
    }

    AppContext *app_ctx = app_ctx_current();

    EcsWorld world;
    ecs_world_init_full_tbl(&world, &app_ctx->arena);

    ECS_COMPONENT(&world, TblPosition);
    ECS_COMPONENT(&world, TblVelocity);
    ECS_COMPONENT(&world, TblHealth);

    EcsTable *root = world.store.root;
    assert_eq(root->type.count, 0);
    assert_eq(root->column_count, 0);

    EcsEntity type_ids_1[] = { ecs_id(TblPosition), ecs_id(TblVelocity) };
    EcsType type_1 = { .array = type_ids_1, .count = 2 };
    EcsTable *table_1 = ecs_table_find_or_create(&world, &type_1);

    assert_eq(table_1->type.count, 2);
    assert_eq(table_1->column_count, 2);

    EcsEntity type_ids_2[] = { ecs_id(TblPosition), ecs_id(TblVelocity), ecs_id(TblHealth) };
    EcsType type_2 = { .array = type_ids_2, .count = 3 };
    EcsTable *table_2 = ecs_table_find_or_create(&world, &type_2);

    assert_eq(table_2->type.count, 3);
    assert_eq(table_2->column_count, 3);

    EcsTable *table_1_again = ecs_table_find_or_create(&world, &type_1);
    assert_true(table_1 == table_1_again);

    EcsEntity e1 = ecs_entity_new(&world);
    EcsEntity e2 = ecs_entity_new(&world);
    EcsEntity e3 = ecs_entity_new(&world);

    i32 row1 = ecs_table_append(&world, table_1, e1);
    i32 row2 = ecs_table_append(&world, table_1, e2);
    i32 row3 = ecs_table_append(&world, table_1, e3);

    assert_eq(row1, 0);
    assert_eq(row2, 1);
    assert_eq(row3, 2);
    assert_eq(table_1->data.count, 3);

    i32 pos_col_idx;
    TblPosition *positions = (TblPosition*)ecs_table_get_column(table_1, ecs_id(TblPosition), &pos_col_idx);
    i32 vel_col_idx;
    TblVelocity *velocities = (TblVelocity*)ecs_table_get_column(table_1, ecs_id(TblVelocity), &vel_col_idx);

    positions[0] = (TblPosition){ 10.0f, 20.0f };
    positions[1] = (TblPosition){ 30.0f, 40.0f };
    positions[2] = (TblPosition){ 50.0f, 60.0f };

    velocities[0] = (TblVelocity){ 1.0f, 2.0f };
    velocities[1] = (TblVelocity){ 3.0f, 4.0f };
    velocities[2] = (TblVelocity){ 5.0f, 6.0f };

    TblPosition *p0 = (TblPosition*)ecs_table_get_component(table_1, 0, pos_col_idx);
    TblPosition *p1 = (TblPosition*)ecs_table_get_component(table_1, 1, pos_col_idx);
    TblPosition *p2 = (TblPosition*)ecs_table_get_component(table_1, 2, pos_col_idx);

    assert_eq((u32)p0->x, 10);
    assert_eq((u32)p1->x, 30);
    assert_eq((u32)p2->x, 50);

    ecs_table_delete(&world, table_1, row2);
    assert_eq(table_1->data.count, 2);

    TblPosition *p_after = (TblPosition*)ecs_table_get_component(table_1, 1, pos_col_idx);
    assert_eq((u32)p_after->x, 50);

    EcsRecord *rec_e3 = ecs_entity_get_record(&world, e3);
    assert_eq(rec_e3->table->id, table_1->id);
    assert_eq(rec_e3->row, 1);

    assert_true(world.store.table_count >= 3);
}
