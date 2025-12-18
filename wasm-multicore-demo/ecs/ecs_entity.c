#include "ecs_entity.h"
#include "lib/assert.h"

#define ECS_INITIAL_DENSE_CAP 1024
#define ECS_INITIAL_PAGE_CAP 16

internal EcsEntityPage* ecs_entity_index_ensure_page(EcsEntityIndex *index, u32 id) {
    i32 page_index = (i32)(id >> ECS_ENTITY_PAGE_BITS);

    if (page_index >= index->page_cap) {
        i32 new_cap = index->page_cap * 2;
        if (new_cap <= page_index) {
            new_cap = page_index + 1;
        }
        EcsEntityPage **new_pages = ARENA_ALLOC_ARRAY(index->arena, EcsEntityPage*, new_cap);
        memset(new_pages, 0, sizeof(EcsEntityPage*) * new_cap);
        if (index->pages) {
            memcpy(new_pages, index->pages, sizeof(EcsEntityPage*) * index->page_count);
        }
        index->pages = new_pages;
        index->page_cap = new_cap;
    }

    if (page_index >= index->page_count) {
        index->page_count = page_index + 1;
    }

    EcsEntityPage *page = index->pages[page_index];
    if (!page) {
        page = ARENA_ALLOC(index->arena, EcsEntityPage);
        memset(page, 0, sizeof(EcsEntityPage));
        index->pages[page_index] = page;
    }

    return page;
}

internal EcsRecord* ecs_entity_index_get_any(EcsEntityIndex *index, EcsEntity entity) {
    u32 id = ecs_entity_index(entity);
    i32 page_index = (i32)(id >> ECS_ENTITY_PAGE_BITS);

    debug_assert(page_index < index->page_count);
    EcsEntityPage *page = index->pages[page_index];
    debug_assert(page != NULL);

    EcsRecord *r = &page->records[id & ECS_ENTITY_PAGE_MASK];
    debug_assert(r->dense != 0);
    return r;
}

internal EcsRecord* ecs_entity_index_try_get(EcsEntityIndex *index, EcsEntity entity) {
    u32 id = ecs_entity_index(entity);
    i32 page_index = (i32)(id >> ECS_ENTITY_PAGE_BITS);

    if (page_index >= index->page_count) {
        return NULL;
    }

    EcsEntityPage *page = index->pages[page_index];
    if (!page) {
        return NULL;
    }

    EcsRecord *r = &page->records[id & ECS_ENTITY_PAGE_MASK];
    if (r->dense == 0) {
        return NULL;
    }

    if (r->dense >= index->alive_count) {
        return NULL;
    }

    if (index->dense[r->dense] != entity) {
        return NULL;
    }

    return r;
}

internal void ecs_entity_index_init(EcsEntityIndex *index, ArenaAllocator *arena) {
    index->arena = arena;
    index->alive_count = 1;
    index->dense_count = 1;
    index->dense_cap = ECS_INITIAL_DENSE_CAP;
    index->dense = ARENA_ALLOC_ARRAY(arena, EcsEntity, ECS_INITIAL_DENSE_CAP);
    memset(index->dense, 0, sizeof(EcsEntity) * ECS_INITIAL_DENSE_CAP);

    index->page_count = 0;
    index->page_cap = ECS_INITIAL_PAGE_CAP;
    index->pages = ARENA_ALLOC_ARRAY(arena, EcsEntityPage*, ECS_INITIAL_PAGE_CAP);
    memset(index->pages, 0, sizeof(EcsEntityPage*) * ECS_INITIAL_PAGE_CAP);

    index->max_id = 0;
}

internal EcsRecord* ecs_entity_index_ensure(EcsEntityIndex *index, EcsEntity entity) {
    u32 id = ecs_entity_index(entity);
    EcsEntityPage *page = ecs_entity_index_ensure_page(index, id);
    EcsRecord *r = &page->records[id & ECS_ENTITY_PAGE_MASK];

    i32 dense = r->dense;
    if (dense) {
        if (dense < index->alive_count) {
            debug_assert(index->dense[dense] == entity);
            return r;
        }
    } else {
        if (index->dense_count >= index->dense_cap) {
            i32 new_cap = index->dense_cap * 2;
            EcsEntity *new_dense = ARENA_ALLOC_ARRAY(index->arena, EcsEntity, new_cap);
            memcpy(new_dense, index->dense, sizeof(EcsEntity) * index->dense_count);
            index->dense = new_dense;
            index->dense_cap = new_cap;
        }

        index->dense[index->dense_count] = entity;
        r->dense = dense = index->dense_count;
        index->dense_count++;
        index->max_id = id > index->max_id ? id : index->max_id;
    }

    debug_assert(dense != 0);

    EcsEntity e_swap = index->dense[index->alive_count];
    u32 swap_id = ecs_entity_index(e_swap);
    i32 swap_page_index = (i32)(swap_id >> ECS_ENTITY_PAGE_BITS);
    EcsEntityPage *swap_page = index->pages[swap_page_index];
    EcsRecord *r_swap = &swap_page->records[swap_id & ECS_ENTITY_PAGE_MASK];

    debug_assert(r_swap->dense == index->alive_count);

    r_swap->dense = dense;
    r->dense = index->alive_count;
    index->dense[dense] = e_swap;
    index->dense[index->alive_count] = entity;
    index->alive_count++;

    return r;
}

internal void ecs_entity_index_remove(EcsEntityIndex *index, EcsEntity entity) {
    EcsRecord *r = ecs_entity_index_try_get(index, entity);
    if (!r) {
        return;
    }

    debug_assert(r->dense < index->alive_count);

    index->alive_count--;

    i32 dense = r->dense;
    EcsEntity e_swap = index->dense[index->alive_count];

    if (e_swap != entity) {
        u32 swap_id = ecs_entity_index(e_swap);
        i32 swap_page_index = (i32)(swap_id >> ECS_ENTITY_PAGE_BITS);
        EcsEntityPage *swap_page = index->pages[swap_page_index];
        EcsRecord *r_swap = &swap_page->records[swap_id & ECS_ENTITY_PAGE_MASK];

        r_swap->dense = dense;
        index->dense[dense] = e_swap;
    }

    r->dense = index->alive_count;

    u32 old_gen = ecs_entity_generation(entity);
    u32 id = ecs_entity_index(entity);
    EcsEntity new_entity = ecs_entity_make(id, old_gen + 1);
    index->dense[index->alive_count] = new_entity;
}

internal b32 ecs_entity_index_is_alive(EcsEntityIndex *index, EcsEntity entity) {
    u32 id = ecs_entity_index(entity);
    i32 page_index = (i32)(id >> ECS_ENTITY_PAGE_BITS);

    if (page_index >= index->page_count) {
        return false;
    }

    EcsEntityPage *page = index->pages[page_index];
    if (!page) {
        return false;
    }

    EcsRecord *r = &page->records[id & ECS_ENTITY_PAGE_MASK];
    if (r->dense == 0) {
        return false;
    }

    if (r->dense >= index->alive_count) {
        return false;
    }

    return index->dense[r->dense] == entity;
}

internal EcsEntity ecs_entity_index_new(EcsEntityIndex *index) {
    EcsEntity entity;

    if (index->alive_count < index->dense_count) {
        entity = index->dense[index->alive_count];
    } else {
        u32 id = (u32)index->dense_count;
        entity = ecs_entity_make(id, 0);
    }

    ecs_entity_index_ensure(index, entity);
    return entity;
}

void ecs_world_init(EcsWorld *world, ArenaAllocator *arena) {
    world->arena = arena;
    ecs_entity_index_init(&world->entity_index, arena);
}

EcsEntity ecs_entity_new(EcsWorld *world) {
    return ecs_entity_index_new(&world->entity_index);
}

void ecs_entity_delete(EcsWorld *world, EcsEntity entity) {
    ecs_entity_index_remove(&world->entity_index, entity);
}

b32 ecs_entity_is_alive(EcsWorld *world, EcsEntity entity) {
    return ecs_entity_index_is_alive(&world->entity_index, entity);
}

b32 ecs_entity_is_valid(EcsWorld *world, EcsEntity entity) {
    if (entity == ECS_ENTITY_INVALID) {
        return false;
    }
    return ecs_entity_is_alive(world, entity);
}

EcsRecord* ecs_entity_get_record(EcsWorld *world, EcsEntity entity) {
    return ecs_entity_index_try_get(&world->entity_index, entity);
}

i32 ecs_entity_count(EcsWorld *world) {
    return world->entity_index.alive_count - 1;
}
