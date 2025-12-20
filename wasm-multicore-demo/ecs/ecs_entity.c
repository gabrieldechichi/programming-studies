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

internal void ecs_entity_index_init(EcsEntityIndex *index, ArenaAllocator *arena, u64 first_id) {
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

    index->max_id = first_id;
}

internal b32 ecs_entity_index_exists(EcsEntityIndex *index, EcsEntity entity) {
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
    return r->dense != 0;
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

    i32 dense = r->dense;
    i32 i_swap = --index->alive_count;
    EcsEntity e_swap = index->dense[i_swap];
    EcsRecord *r_swap = ecs_entity_index_get_any(index, e_swap);

    debug_assert(r_swap->dense == i_swap);

    r_swap->dense = dense;
    r->table = NULL;
    r->row = 0;
    r->dense = i_swap;
    index->dense[dense] = e_swap;

    u32 old_gen = ecs_entity_generation(entity);
    u32 id = ecs_entity_index(entity);
    index->dense[i_swap] = ecs_entity_make(id, old_gen + 1);
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
    if (index->alive_count < index->dense_count) {
        return index->dense[index->alive_count++];
    }

    u32 id = (u32)(++index->max_id);

    debug_assert(!ecs_entity_index_exists(index, id));

    if (index->dense_count >= index->dense_cap) {
        i32 new_cap = index->dense_cap * 2;
        EcsEntity *new_dense = ARENA_ALLOC_ARRAY(index->arena, EcsEntity, new_cap);
        memcpy(new_dense, index->dense, sizeof(EcsEntity) * index->dense_count);
        index->dense = new_dense;
        index->dense_cap = new_cap;
    }

    index->dense[index->dense_count] = id;

    EcsEntityPage *page = ecs_entity_index_ensure_page(index, id);
    EcsRecord *r = &page->records[id & ECS_ENTITY_PAGE_MASK];
    r->dense = index->alive_count++;
    index->dense_count++;

    debug_assert(index->alive_count == index->dense_count);

    return id;
}

void ecs_world_init(EcsWorld *world, ArenaAllocator *arena) {
    world->arena = arena;
    ecs_entity_index_init(&world->entity_index, arena, ECS_FIRST_USER_ENTITY_ID);
    world->last_component_id = ECS_FIRST_USER_COMPONENT_ID;
    world->type_info = ARENA_ALLOC_ARRAY(arena, EcsTypeInfo, ECS_HI_COMPONENT_ID);
    memset(world->type_info, 0, sizeof(EcsTypeInfo) * ECS_HI_COMPONENT_ID);
    world->component_records = ARENA_ALLOC_ARRAY(arena, EcsComponentRecord, ECS_HI_COMPONENT_ID);
    memset(world->component_records, 0, sizeof(EcsComponentRecord) * ECS_HI_COMPONENT_ID);
    world->type_info_count = 0;
    world->cached_queries = NULL;
    world->cached_query_count = 0;
    world->cached_query_cap = 0;
    world->systems = NULL;
    world->system_count = 0;
    world->system_cap = 0;
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

b32 ecs_entity_exists(EcsWorld *world, EcsEntity entity) {
    return ecs_entity_index_exists(&world->entity_index, entity);
}

EcsEntity ecs_entity_new_low_id(EcsWorld *world) {
    EcsEntity e = 0;

    if (world->last_component_id < ECS_HI_COMPONENT_ID) {
        do {
            e = world->last_component_id++;
        } while (ecs_entity_exists(world, e) && e < ECS_HI_COMPONENT_ID);
    }

    if (!e || e >= ECS_HI_COMPONENT_ID) {
        e = ecs_entity_new(world);
    } else {
        ecs_entity_index_ensure(&world->entity_index, e);
    }

    return e;
}

EcsEntity ecs_component_register(EcsWorld *world, u32 size, u32 alignment, const char *name) {
    EcsEntity e = ecs_entity_new_low_id(world);

    debug_assert(ecs_entity_index(e) < ECS_HI_COMPONENT_ID);

    u32 id = ecs_entity_index(e);
    EcsTypeInfo *ti = &world->type_info[id];
    ti->size = size;
    ti->alignment = alignment;
    ti->component = e;
    ti->name = name;
    world->type_info_count++;

    EcsComponentRecord *cr = &world->component_records[id];
    cr->id = e;
    cr->type_info = ti;
    cr->first = NULL;
    cr->last = NULL;
    cr->table_count = 0;

    return e;
}

const EcsTypeInfo* ecs_type_info_get(EcsWorld *world, EcsEntity component) {
    u32 id = ecs_entity_index(component);
    if (id >= ECS_HI_COMPONENT_ID) {
        return NULL;
    }

    EcsTypeInfo *ti = &world->type_info[id];
    if (ti->component == 0) {
        return NULL;
    }

    return ti;
}

EcsComponentRecord* ecs_component_record_get(EcsWorld *world, EcsEntity component) {
    u32 id = ecs_entity_index(component);
    if (id >= ECS_HI_COMPONENT_ID) {
        return NULL;
    }

    EcsComponentRecord *cr = &world->component_records[id];
    if (cr->id == 0) {
        return NULL;
    }

    return cr;
}

EcsTableRecord* ecs_component_record_get_table(EcsComponentRecord *cr, EcsTable *table) {
    if (!cr) {
        return NULL;
    }

    for (EcsTableRecord *tr = cr->first; tr != NULL; tr = tr->next) {
        if (tr->table == table) {
            return tr;
        }
    }

    return NULL;
}
