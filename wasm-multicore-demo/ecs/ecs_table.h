#ifndef ECS_TABLE_H
#define ECS_TABLE_H

#include "ecs_entity.h"
#include "lib/hash.h"

#define ECS_TABLE_INITIAL_CAPACITY 8
#define ECS_TABLE_MAP_INITIAL_CAPACITY 64

typedef struct EcsType {
    EcsEntity *array;
    i32 count;
} EcsType;

typedef struct EcsColumn {
    void *data;
    EcsTypeInfo *ti;
} EcsColumn;

typedef struct EcsTableData {
    EcsEntity *entities;
    EcsColumn *columns;
    i32 count;
    i32 size;
} EcsTableData;

// TODO: add EcsTableDiff *diff (added/removed component arrays) for faster moves
typedef struct EcsGraphEdge {
    EcsEntity id;
    struct EcsTable *to;
} EcsGraphEdge;

// TODO: use hash map for hi edges instead of array (O(1) vs O(n) lookup)
typedef struct EcsGraphEdges {
    EcsGraphEdge *lo;
    EcsGraphEdge *hi;
    i32 hi_count;
    i32 hi_cap;
} EcsGraphEdges;

typedef struct EcsGraphNode {
    EcsGraphEdges add;
    EcsGraphEdges remove;
} EcsGraphNode;

struct EcsTable {
    u64 id;
    EcsType type;
    EcsTableData data;
    EcsGraphNode node;
    u64 bloom_filter;
    i32 *dirty_state;
    i16 column_count;
    i16 *column_map;
};

typedef struct EcsTableMapBucket {
    EcsType *keys;
    EcsTable **values;
    i32 count;
    i32 capacity;
} EcsTableMapBucket;

typedef struct EcsTableMap {
    EcsTableMapBucket *buckets;
    i32 bucket_count;
    ArenaAllocator *arena;
} EcsTableMap;

force_inline u64 ecs_type_hash(const EcsType *type) {
    return flecs_hash(type->array, type->count * (i32)sizeof(EcsEntity));
}

force_inline u64 ecs_bloom_bit(EcsEntity component) {
    return 1ull << (ecs_entity_index(component) % 64);
}

force_inline i32 ecs_type_compare(const EcsType *type_1, const EcsType *type_2) {
    if (type_1->count != type_2->count) {
        return (type_1->count > type_2->count) - (type_1->count < type_2->count);
    }

    for (i32 i = 0; i < type_1->count; i++) {
        if (type_1->array[i] != type_2->array[i]) {
            return (type_1->array[i] > type_2->array[i]) - (type_1->array[i] < type_2->array[i]);
        }
    }
    return 0;
}

void ecs_table_map_init(EcsTableMap *map, ArenaAllocator *arena);
EcsTable* ecs_table_map_get(EcsTableMap *map, const EcsType *type);
void ecs_table_map_set(EcsTableMap *map, const EcsType *type, EcsTable *table);

void ecs_table_init(EcsWorld *world, EcsTable *table, const EcsType *type);
i32 ecs_table_append(EcsWorld *world, EcsTable *table, EcsEntity entity);
void ecs_table_delete(EcsWorld *world, EcsTable *table, i32 row);
void ecs_table_move(EcsWorld *world, EcsEntity entity, EcsTable *dst_table, EcsTable *src_table, i32 src_row);
void* ecs_table_get_column(EcsTable *table, EcsEntity component, i32 *out_column_index);
void* ecs_table_get_component(EcsTable *table, i32 row, i32 column_index);
i32 ecs_table_get_column_index(EcsTable *table, EcsEntity component);

EcsTable* ecs_table_find_or_create(EcsWorld *world, const EcsType *type);
EcsTable* ecs_table_traverse_add(EcsWorld *world, EcsTable *table, EcsEntity component);
EcsTable* ecs_table_traverse_remove(EcsWorld *world, EcsTable *table, EcsEntity component);

void ecs_store_init(EcsWorld *world);

void ecs_add(EcsWorld *world, EcsEntity entity, EcsEntity component);
void ecs_remove(EcsWorld *world, EcsEntity entity, EcsEntity component);
b32 ecs_has(EcsWorld *world, EcsEntity entity, EcsEntity component);
void* ecs_get(EcsWorld *world, EcsEntity entity, EcsEntity component);
void* ecs_get_mut(EcsWorld *world, EcsEntity entity, EcsEntity component);
void ecs_set_ptr(EcsWorld *world, EcsEntity entity, EcsEntity component, const void *ptr);

#define ecs_set(world, entity, T, ...) \
    do { \
        T __temp = __VA_ARGS__; \
        ecs_set_ptr((world), (entity), ecs_id(T), &__temp); \
    } while(0)

#define ecs_get_component(world, entity, T) \
    ((T*)ecs_get((world), (entity), ecs_id(T)))

#define ECS_QUERY_MAX_TERMS 16

typedef enum {
    EcsOperAnd = 0,
    EcsOperNot,
    EcsOperOptional,
    EcsOperOr,
} EcsOperKind;

typedef enum {
    EcsInOutDefault = 0,
    EcsIn,
    EcsOut,
    EcsInOut,
    EcsInOutNone,
} EcsInOutKind;

typedef struct EcsTerm {
    EcsEntity id;
    i16 oper;
    i16 inout;
    i8 field_index;
    i8 or_chain_length;
} EcsTerm;

typedef struct EcsQueryCacheMatch {
    EcsTable *table;
    i16 columns[ECS_QUERY_MAX_TERMS];
    u32 set_fields;
    i32 *monitor;
    struct EcsQueryCacheMatch *next;
} EcsQueryCacheMatch;

typedef struct EcsQueryCache {
    EcsQueryCacheMatch *first;
    EcsQueryCacheMatch *last;
    i32 match_count;
} EcsQueryCache;

struct EcsQuery {
    EcsWorld *world;
    EcsTerm terms[ECS_QUERY_MAX_TERMS];
    i32 term_count;
    i32 field_count;
    u64 bloom_filter;
    u32 read_fields;
    u32 write_fields;
    EcsQueryCache cache;
    b32 is_cached;
};

typedef struct EcsIter {
    EcsWorld *world;
    EcsQuery *query;

    EcsTable *table;
    i32 offset;
    i32 count;
    EcsEntity *entities;

    i16 columns[ECS_QUERY_MAX_TERMS];
    u32 set_fields;

    f32 delta_time;
    void *ctx;

    EcsTableRecord *cur;
    EcsQueryCacheMatch *cache_cur;
} EcsIter;

#define ECS_MAX_SYSTEM_DEPS 16

typedef void (*EcsSystemCallback)(EcsIter *it);

typedef struct EcsSystem {
    EcsEntity id;
    EcsQuery query;
    EcsSystemCallback callback;
    void *ctx;
    const char *name;

    struct EcsSystem **depends_on;
    i32 depends_on_count;
    i32 depends_on_cap;

    void *task_handles;
    b32 main_thread_only;
} EcsSystem;

typedef struct EcsSystemDesc {
    EcsTerm *terms;
    i32 term_count;
    EcsSystemCallback callback;
    void *ctx;
    const char *name;
    b32 main_thread_only;
} EcsSystemDesc;

typedef struct EcsSystemRunData {
    EcsSystem *sys;
    f32 delta_time;
    u8 thread_idx;
} EcsSystemRunData;

force_inline EcsTerm ecs_term(EcsEntity id) {
    EcsTerm t = {0};
    t.id = id;
    t.oper = EcsOperAnd;
    t.inout = EcsInOutDefault;
    t.field_index = -1;
    t.or_chain_length = 0;
    return t;
}

force_inline EcsTerm ecs_term_w_inout(EcsEntity id, EcsInOutKind inout) {
    EcsTerm t = {0};
    t.id = id;
    t.oper = EcsOperAnd;
    t.inout = (i16)inout;
    t.field_index = -1;
    t.or_chain_length = 0;
    return t;
}

force_inline EcsTerm ecs_term_in(EcsEntity id) {
    return ecs_term_w_inout(id, EcsIn);
}

force_inline EcsTerm ecs_term_out(EcsEntity id) {
    return ecs_term_w_inout(id, EcsOut);
}

force_inline EcsTerm ecs_term_inout(EcsEntity id) {
    return ecs_term_w_inout(id, EcsInOut);
}

force_inline EcsTerm ecs_term_none(EcsEntity id) {
    return ecs_term_w_inout(id, EcsInOutNone);
}

force_inline EcsTerm ecs_term_not(EcsEntity id) {
    EcsTerm t = {0};
    t.id = id;
    t.oper = EcsOperNot;
    t.inout = EcsInOutNone;
    t.field_index = -1;
    t.or_chain_length = 0;
    return t;
}

force_inline EcsTerm ecs_term_optional(EcsEntity id) {
    EcsTerm t = {0};
    t.id = id;
    t.oper = EcsOperOptional;
    t.inout = EcsInOutDefault;
    t.field_index = -1;
    t.or_chain_length = 0;
    return t;
}

force_inline EcsTerm ecs_term_or(EcsEntity id, i8 chain_length) {
    EcsTerm t = {0};
    t.id = id;
    t.oper = EcsOperOr;
    t.inout = EcsInOutDefault;
    t.field_index = -1;
    t.or_chain_length = chain_length;
    return t;
}

void ecs_query_init(EcsQuery *query, EcsWorld *world, EcsEntity *terms, i32 term_count);
void ecs_query_init_terms(EcsQuery *query, EcsWorld *world, EcsTerm *terms, i32 term_count);
EcsIter ecs_query_iter(EcsQuery *query);
b32 ecs_iter_next(EcsIter *it);
void* ecs_iter_field(EcsIter *it, i32 field_index);
i32 ecs_iter_field_column(EcsIter *it, i32 field_index);

void ecs_query_cache_init(EcsQuery *query);
void ecs_query_cache_populate(EcsQuery *query);
void ecs_query_cache_add_table(EcsQuery *query, EcsTable *table);
void ecs_query_cache_remove_table(EcsQuery *query, EcsTable *table);
b32 ecs_query_table_matches(EcsQuery *query, EcsTable *table, i16 *out_columns, u32 *out_set_fields);

void ecs_table_mark_dirty(EcsTable *table, i32 column);
b32 ecs_query_changed(EcsQuery *query);
b32 ecs_iter_changed(EcsIter *it);
void ecs_query_sync(EcsQuery *query);
void ecs_iter_sync(EcsIter *it);

EcsSystem* ecs_system_init(EcsWorld *world, const EcsSystemDesc *desc);
EcsSystem* ecs_system_get(EcsWorld *world, i32 index);
void ecs_system_depends_on(EcsSystem *system, EcsSystem *dependency);
b32 ecs_systems_conflict(EcsSystem *writer, EcsSystem *reader);
void ecs_world_compute_system_dependencies(EcsWorld *world);
void ecs_progress(EcsWorld *world, f32 delta_time);

#define ecs_field(it, T, index) ((T*)ecs_iter_field((it), (index)))
#define ecs_field_is_set(it, index) (((it)->set_fields & (1u << (index))) != 0)

#define ECS_SYSTEM(world, callback_fn, terms_arr, terms_count) \
    ecs_system_init((world), &(EcsSystemDesc){ \
        .terms = (terms_arr), \
        .term_count = (terms_count), \
        .callback = (callback_fn), \
        .name = #callback_fn, \
    })

#endif
