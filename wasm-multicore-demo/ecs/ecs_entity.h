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

typedef u64 EcsEntity;

typedef struct EcsTable EcsTable;

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

typedef struct EcsWorld {
    EcsEntityIndex entity_index;
    ArenaAllocator *arena;
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
void ecs_entity_delete(EcsWorld *world, EcsEntity entity);
b32 ecs_entity_is_alive(EcsWorld *world, EcsEntity entity);
b32 ecs_entity_is_valid(EcsWorld *world, EcsEntity entity);
EcsRecord* ecs_entity_get_record(EcsWorld *world, EcsEntity entity);
i32 ecs_entity_count(EcsWorld *world);

#endif
