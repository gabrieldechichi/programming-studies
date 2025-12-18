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

typedef struct EcsGraphEdge {
    EcsEntity id;
    struct EcsTable *to;
} EcsGraphEdge;

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

#endif
