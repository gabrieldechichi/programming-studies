#include "context.h"
#include "lib/thread_context.h"
#include "lib/typedefs.h"
#include "os/os.h"
#include "lib/math.h"
#include "cube.h"
#include "renderer.h"
#include "camera.h"
#include "app.h"

#include "ecs/ecs_entity.c"
#include "ecs/ecs_table.c"

typedef struct { f32 value; } Alpha;
typedef struct { f32 value; } Beta;
typedef struct { f32 value; } Gamma;
typedef struct { f32 value; } Delta;

global EcsWorld g_world;
global EcsSystem *g_sys_a;
global EcsSystem *g_sys_b;
global EcsSystem *g_sys_c;
global EcsSystem *g_sys_d;
global EcsSystem *g_sys_e;
global u32 g_frame_count = 0;

void ecs_world_init_full(EcsWorld *world, ArenaAllocator *arena) {
    ecs_world_init(world, arena);
    ecs_store_init(world);
}

void SystemA(EcsIter *it) {
    Alpha *a = ecs_field(it, Alpha, 0);
    for (i32 i = 0; i < it->count; i++) {
        a[i].value += 1.0f;
    }
}

void SystemB(EcsIter *it) {
    Alpha *a = ecs_field(it, Alpha, 0);
    Beta *b = ecs_field(it, Beta, 1);
    for (i32 i = 0; i < it->count; i++) {
        b[i].value = a[i].value * 2.0f;
    }
}

void SystemC(EcsIter *it) {
    Beta *b = ecs_field(it, Beta, 0);
    Gamma *g = ecs_field(it, Gamma, 1);
    for (i32 i = 0; i < it->count; i++) {
        g[i].value = b[i].value + 10.0f;
    }
}

void SystemD(EcsIter *it) {
    Alpha *a = ecs_field(it, Alpha, 0);
    Beta *b = ecs_field(it, Beta, 1);
    Gamma *g = ecs_field(it, Gamma, 2);
    Delta *d = ecs_field(it, Delta, 3);
    for (i32 i = 0; i < it->count; i++) {
        d[i].value = a[i].value + b[i].value + g[i].value;
    }
}

void SystemE(EcsIter *it) {
    Alpha *a = ecs_field(it, Alpha, 0);
    Beta *b = ecs_field(it, Beta, 1);
    UNUSED(a);
    UNUSED(b);
}

void app_init(AppMemory *memory) {
    UNUSED(memory);

    if (!is_main_thread()) {
        return;
    }

    AppContext *app_ctx = app_ctx_current();
    ThreadContext *tctx = tctx_current();

    ecs_world_init_full(&g_world, &app_ctx->arena);

    LOG_INFO("=== ECS Dependency Test ===");
    LOG_INFO("Thread count: %", FMT_UINT(tctx->thread_count));

    ECS_COMPONENT(&g_world, Alpha);
    ECS_COMPONENT(&g_world, Beta);
    ECS_COMPONENT(&g_world, Gamma);
    ECS_COMPONENT(&g_world, Delta);

    for (i32 i = 0; i < 100; i++) {
        EcsEntity e = ecs_entity_new(&g_world);
        ecs_set(&g_world, e, Alpha, { .value = 0.0f });
        ecs_set(&g_world, e, Beta, { .value = 0.0f });
        ecs_set(&g_world, e, Gamma, { .value = 0.0f });
        ecs_set(&g_world, e, Delta, { .value = 0.0f });
    }

    LOG_INFO("--- Expected dependencies ---");
    LOG_INFO("A: writes Alpha -> 0 deps");
    LOG_INFO("B: reads Alpha, writes Beta -> depends on A");
    LOG_INFO("C: reads Beta, writes Gamma -> depends on B");
    LOG_INFO("D: reads Alpha/Beta/Gamma, writes Delta -> depends on A, B, C");
    LOG_INFO("E: reads Alpha/Beta -> depends on A, B");

    LOG_INFO("--- Registering systems ---");

    EcsTerm terms_a[] = { ecs_term_out(ecs_id(Alpha)) };
    g_sys_a = ECS_SYSTEM(&g_world, SystemA, terms_a, 1);

    EcsTerm terms_b[] = { ecs_term_in(ecs_id(Alpha)), ecs_term_out(ecs_id(Beta)) };
    g_sys_b = ECS_SYSTEM(&g_world, SystemB, terms_b, 2);

    EcsTerm terms_c[] = { ecs_term_in(ecs_id(Beta)), ecs_term_out(ecs_id(Gamma)) };
    g_sys_c = ECS_SYSTEM(&g_world, SystemC, terms_c, 2);

    EcsTerm terms_d[] = {
        ecs_term_in(ecs_id(Alpha)),
        ecs_term_in(ecs_id(Beta)),
        ecs_term_in(ecs_id(Gamma)),
        ecs_term_out(ecs_id(Delta))
    };
    g_sys_d = ECS_SYSTEM(&g_world, SystemD, terms_d, 4);

    EcsTerm terms_e[] = { ecs_term_in(ecs_id(Alpha)), ecs_term_in(ecs_id(Beta)) };
    g_sys_e = ECS_SYSTEM(&g_world, SystemE, terms_e, 2);

    LOG_INFO("--- Actual dependencies ---");
    LOG_INFO("SystemA deps: % (expected 0)", FMT_UINT(g_sys_a->depends_on_count));
    LOG_INFO("SystemB deps: % (expected 1: A)", FMT_UINT(g_sys_b->depends_on_count));
    LOG_INFO("SystemC deps: % (expected 1: B)", FMT_UINT(g_sys_c->depends_on_count));
    LOG_INFO("SystemD deps: % (expected 3: A,B,C)", FMT_UINT(g_sys_d->depends_on_count));
    LOG_INFO("SystemE deps: % (expected 2: A,B)", FMT_UINT(g_sys_e->depends_on_count));

    LOG_INFO("=== Test complete ===");
}

void app_update_and_render(AppMemory *memory) {
    UNUSED(memory);

    f32 delta_time = 0.016f;
    ecs_progress(&g_world, delta_time);

    if (is_main_thread()) {
        g_frame_count++;
        if (g_frame_count % 120 == 0) {
            LOG_INFO("Frame %", FMT_UINT(g_frame_count));
        }
    }
}
