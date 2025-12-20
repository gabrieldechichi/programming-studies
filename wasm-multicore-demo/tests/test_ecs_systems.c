typedef struct { f32 value; } SysAlpha;
typedef struct { f32 value; } SysBeta;
typedef struct { f32 value; } SysGamma;
typedef struct { f32 value; } SysDelta;

void ecs_world_init_full_sys(EcsWorld *world, ArenaAllocator *arena) {
    ecs_world_init(world, arena);
    ecs_store_init(world);
}

void SysSystemA(EcsIter *it) {
    SysAlpha *a = ecs_field(it, SysAlpha, 0);
    for (i32 i = 0; i < it->count; i++) {
        a[i].value += 1.0f;
    }
}

void SysSystemB(EcsIter *it) {
    SysAlpha *a = ecs_field(it, SysAlpha, 0);
    SysBeta *b = ecs_field(it, SysBeta, 1);
    for (i32 i = 0; i < it->count; i++) {
        b[i].value = a[i].value * 2.0f;
    }
}

void SysSystemC(EcsIter *it) {
    SysBeta *b = ecs_field(it, SysBeta, 0);
    SysGamma *g = ecs_field(it, SysGamma, 1);
    for (i32 i = 0; i < it->count; i++) {
        g[i].value = b[i].value + 10.0f;
    }
}

void SysSystemD(EcsIter *it) {
    SysAlpha *a = ecs_field(it, SysAlpha, 0);
    SysBeta *b = ecs_field(it, SysBeta, 1);
    SysGamma *g = ecs_field(it, SysGamma, 2);
    SysDelta *d = ecs_field(it, SysDelta, 3);
    for (i32 i = 0; i < it->count; i++) {
        d[i].value = a[i].value + b[i].value + g[i].value;
    }
}

void SysSystemE(EcsIter *it) {
    SysAlpha *a = ecs_field(it, SysAlpha, 0);
    SysBeta *b = ecs_field(it, SysBeta, 1);
    UNUSED(a);
    UNUSED(b);
}

void test_ecs_systems(void) {
    if (!is_main_thread()) {
        return;
    }

    AppContext *app_ctx = app_ctx_current();

    EcsWorld world;
    ecs_world_init_full_sys(&world, &app_ctx->arena);

    ECS_COMPONENT(&world, SysAlpha);
    ECS_COMPONENT(&world, SysBeta);
    ECS_COMPONENT(&world, SysGamma);
    ECS_COMPONENT(&world, SysDelta);

    for (i32 i = 0; i < 100; i++) {
        EcsEntity e = ecs_entity_new(&world);
        ecs_set(&world, e, SysAlpha, { .value = 0.0f });
        ecs_set(&world, e, SysBeta, { .value = 0.0f });
        ecs_set(&world, e, SysGamma, { .value = 0.0f });
        ecs_set(&world, e, SysDelta, { .value = 0.0f });
    }

    EcsTerm terms_a[] = { ecs_term_out(ecs_id(SysAlpha)) };
    EcsSystem *sys_a = ECS_SYSTEM(&world, SysSystemA, terms_a, 1);

    EcsTerm terms_b[] = { ecs_term_in(ecs_id(SysAlpha)), ecs_term_out(ecs_id(SysBeta)) };
    EcsSystem *sys_b = ECS_SYSTEM(&world, SysSystemB, terms_b, 2);

    EcsTerm terms_c[] = { ecs_term_in(ecs_id(SysBeta)), ecs_term_out(ecs_id(SysGamma)) };
    EcsSystem *sys_c = ECS_SYSTEM(&world, SysSystemC, terms_c, 2);

    EcsTerm terms_d[] = {
        ecs_term_in(ecs_id(SysAlpha)),
        ecs_term_in(ecs_id(SysBeta)),
        ecs_term_in(ecs_id(SysGamma)),
        ecs_term_out(ecs_id(SysDelta))
    };
    EcsSystem *sys_d = ECS_SYSTEM(&world, SysSystemD, terms_d, 4);

    EcsTerm terms_e[] = { ecs_term_in(ecs_id(SysAlpha)), ecs_term_in(ecs_id(SysBeta)) };
    EcsSystem *sys_e = ECS_SYSTEM(&world, SysSystemE, terms_e, 2);

    assert_eq(sys_a->depends_on_count, 0);
    assert_eq(sys_b->depends_on_count, 1);
    assert_eq(sys_c->depends_on_count, 1);
    assert_eq(sys_d->depends_on_count, 3);
    assert_eq(sys_e->depends_on_count, 2);

    assert_eq(world.system_count, 5);
}
