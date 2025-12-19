#ifndef ECS_ENTITY_H
#define ECS_ENTITY_H

#include "lib/typedefs.h"
#include "lib/memory.h"

#define ECS_ENTITY_PAGE_BITS 10
#define ECS_ENTITY_PAGE_SIZE (1 << ECS_ENTITY_PAGE_BITS)
#define ECS_ENTITY_PAGE_MASK (ECS_ENTITY_PAGE_SIZE - 1)

#define ECS_GENERATION_SHIFT 32
#define ECS_GENERATION_MASK 0xFFFFFFFF00000000ULL
#define ECS_INDEX_MASK 0x00000000FFFFFFFFULL

#define ECS_ENTITY_INVALID 0

#define ECS_HI_COMPONENT_ID 256
#define ECS_FIRST_USER_COMPONENT_ID 8
#define ECS_FIRST_USER_ENTITY_ID (ECS_HI_COMPONENT_ID + 128)

typedef u64 EcsEntity;

typedef struct EcsTable EcsTable;
typedef struct EcsTableMap EcsTableMap;
typedef struct EcsQuery EcsQuery;

typedef struct EcsRecord {
    EcsTable *table;
    u32 row;
    i32 dense;
} EcsRecord;

typedef struct EcsEntityPage {
    EcsRecord records[ECS_ENTITY_PAGE_SIZE];
} EcsEntityPage;

typedef struct EcsEntityIndex {
    EcsEntity *dense;
    EcsEntityPage **pages;
    i32 dense_count;
    i32 dense_cap;
    i32 alive_count;
    i32 page_count;
    i32 page_cap;
    u64 max_id;
    ArenaAllocator *arena;
} EcsEntityIndex;

// TODO: add type hooks for complex types (ctor/dtor/copy/move callbacks)
// void (*ctor)(void *ptr, i32 count, const EcsTypeInfo *ti);
// void (*dtor)(void *ptr, i32 count, const EcsTypeInfo *ti);
// void (*copy)(void *dst, const void *src, i32 count, const EcsTypeInfo *ti);
// void (*move)(void *dst, void *src, i32 count, const EcsTypeInfo *ti);
typedef struct EcsTypeInfo {
    u32 size;
    u32 alignment;
    EcsEntity component;
    const char *name;
} EcsTypeInfo;

typedef struct EcsTableRecord {
    EcsTable *table;
    i16 column;
    i16 type_index;
    struct EcsTableRecord *prev;
    struct EcsTableRecord *next;
} EcsTableRecord;

typedef struct EcsComponentRecord {
    EcsEntity id;
    const EcsTypeInfo *type_info;
    EcsTableRecord *first;
    EcsTableRecord *last;
    i32 table_count;
} EcsComponentRecord;


typedef struct EcsStore {
    EcsTable *tables;
    i32 table_count;
    i32 table_cap;
    EcsTable *root;
    EcsTableMap *table_map;
} EcsStore;

typedef struct EcsWorld {
    EcsEntityIndex entity_index;
    ArenaAllocator *arena;
    EcsEntity last_component_id;
    EcsTypeInfo *type_info;
    EcsComponentRecord *component_records;
    i32 type_info_count;
    EcsStore store;
    EcsQuery **cached_queries;
    i32 cached_query_count;
    i32 cached_query_cap;
} EcsWorld;

force_inline u32 ecs_entity_index(EcsEntity entity) {
    return (u32)(entity & ECS_INDEX_MASK);
}

force_inline u32 ecs_entity_generation(EcsEntity entity) {
    return (u32)((entity & ECS_GENERATION_MASK) >> ECS_GENERATION_SHIFT);
}

force_inline EcsEntity ecs_entity_make(u32 index, u32 generation) {
    return ((u64)generation << ECS_GENERATION_SHIFT) | (u64)index;
}

void ecs_world_init(EcsWorld *world, ArenaAllocator *arena);
EcsEntity ecs_entity_new(EcsWorld *world);
EcsEntity ecs_entity_new_low_id(EcsWorld *world);
void ecs_entity_delete(EcsWorld *world, EcsEntity entity);
b32 ecs_entity_is_alive(EcsWorld *world, EcsEntity entity);
b32 ecs_entity_is_valid(EcsWorld *world, EcsEntity entity);
b32 ecs_entity_exists(EcsWorld *world, EcsEntity entity);
EcsRecord* ecs_entity_get_record(EcsWorld *world, EcsEntity entity);
i32 ecs_entity_count(EcsWorld *world);

EcsEntity ecs_component_register(EcsWorld *world, u32 size, u32 alignment, const char *name);
const EcsTypeInfo* ecs_type_info_get(EcsWorld *world, EcsEntity component);
EcsComponentRecord* ecs_component_record_get(EcsWorld *world, EcsEntity component);
EcsTableRecord* ecs_component_record_get_table(EcsComponentRecord *cr, EcsTable *table);

#define ecs_id(T) FLECS_ID##T##ID_

#define ECS_COMPONENT(world, T) \
    EcsEntity ecs_id(T) = ecs_component_register( \
        (world), \
        sizeof(T), \
        _Alignof(T), \
        #T \
    )

#endif
