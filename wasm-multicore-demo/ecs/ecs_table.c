#include "ecs_table.h"
#include "lib/assert.h"

void ecs_table_map_init(EcsTableMap *map, ArenaAllocator *arena) {
    map->arena = arena;
    map->bucket_count = ECS_TABLE_MAP_INITIAL_CAPACITY;
    map->buckets = ARENA_ALLOC_ARRAY(arena, EcsTableMapBucket, map->bucket_count);
    memset(map->buckets, 0, sizeof(EcsTableMapBucket) * map->bucket_count);
}

EcsTable* ecs_table_map_get(EcsTableMap *map, const EcsType *type) {
    u64 hash = ecs_type_hash(type);
    i32 bucket_idx = (i32)(hash % (u64)map->bucket_count);
    EcsTableMapBucket *bucket = &map->buckets[bucket_idx];

    for (i32 i = 0; i < bucket->count; i++) {
        if (ecs_type_compare(&bucket->keys[i], type) == 0) {
            return bucket->values[i];
        }
    }

    return NULL;
}

void ecs_table_map_set(EcsTableMap *map, const EcsType *type, EcsTable *table) {
    u64 hash = ecs_type_hash(type);
    i32 bucket_idx = (i32)(hash % (u64)map->bucket_count);
    EcsTableMapBucket *bucket = &map->buckets[bucket_idx];

    for (i32 i = 0; i < bucket->count; i++) {
        if (ecs_type_compare(&bucket->keys[i], type) == 0) {
            bucket->values[i] = table;
            return;
        }
    }

    if (bucket->count >= bucket->capacity) {
        i32 new_cap = bucket->capacity == 0 ? 4 : bucket->capacity * 2;
        EcsType *new_keys = ARENA_ALLOC_ARRAY(map->arena, EcsType, new_cap);
        EcsTable **new_values = ARENA_ALLOC_ARRAY(map->arena, EcsTable*, new_cap);

        if (bucket->keys) {
            memcpy(new_keys, bucket->keys, sizeof(EcsType) * bucket->count);
            memcpy(new_values, bucket->values, sizeof(EcsTable*) * bucket->count);
        }

        bucket->keys = new_keys;
        bucket->values = new_values;
        bucket->capacity = new_cap;
    }

    EcsType *key = &bucket->keys[bucket->count];
    key->count = type->count;
    key->array = ARENA_ALLOC_ARRAY(map->arena, EcsEntity, type->count);
    memcpy(key->array, type->array, sizeof(EcsEntity) * type->count);

    bucket->values[bucket->count] = table;
    bucket->count++;
}

internal i32 ecs_type_index_of(const EcsType *type, EcsEntity component) {
    for (i32 i = 0; i < type->count; i++) {
        if (type->array[i] == component) {
            return i;
        }
    }
    return -1;
}

void ecs_table_init(EcsWorld *world, EcsTable *table, const EcsType *type) {
    table->type.count = type ? type->count : 0;

    if (table->type.count > 0) {
        table->type.array = ARENA_ALLOC_ARRAY(world->arena, EcsEntity, type->count);
        memcpy(table->type.array, type->array, sizeof(EcsEntity) * type->count);
    } else {
        table->type.array = NULL;
    }

    table->component_map = ARENA_ALLOC_ARRAY(world->arena, i16, ECS_HI_COMPONENT_ID);
    memset(table->component_map, 0, sizeof(i16) * ECS_HI_COMPONENT_ID);

    for (i32 i = 0; i < table->type.count; i++) {
        EcsEntity comp = table->type.array[i];
        u32 comp_id = ecs_entity_index(comp);
        if (comp_id < ECS_HI_COMPONENT_ID) {
            table->component_map[comp_id] = (i16)(-(i + 1));
        }
    }

    i32 column_count = 0;
    for (i32 i = 0; i < table->type.count; i++) {
        EcsEntity comp = table->type.array[i];
        const EcsTypeInfo *ti = ecs_type_info_get(world, comp);
        if (ti && ti->size > 0) {
            column_count++;
        }
    }

    table->column_count = (i16)column_count;

    if (column_count > 0) {
        table->data.columns = ARENA_ALLOC_ARRAY(world->arena, EcsColumn, column_count);
        memset(table->data.columns, 0, sizeof(EcsColumn) * column_count);

        i32 col_idx = 0;
        for (i32 i = 0; i < table->type.count; i++) {
            EcsEntity comp = table->type.array[i];
            const EcsTypeInfo *ti = ecs_type_info_get(world, comp);
            if (ti && ti->size > 0) {
                table->data.columns[col_idx].ti = (EcsTypeInfo*)ti;

                u32 comp_id = ecs_entity_index(comp);
                if (comp_id < ECS_HI_COMPONENT_ID) {
                    table->component_map[comp_id] = (i16)(col_idx + 1);
                }

                col_idx++;
            }
        }
    } else {
        table->data.columns = NULL;
    }

    table->data.entities = NULL;
    table->data.count = 0;
    table->data.size = 0;
}

i32 ecs_table_append(EcsWorld *world, EcsTable *table, EcsEntity entity) {
    i32 count = table->data.count;
    i32 size = table->data.size;

    if (count >= size) {
        i32 new_size = size == 0 ? ECS_TABLE_INITIAL_CAPACITY : size * 2;

        EcsEntity *new_entities = ARENA_ALLOC_ARRAY(world->arena, EcsEntity, new_size);
        if (table->data.entities) {
            memcpy(new_entities, table->data.entities, sizeof(EcsEntity) * count);
        }
        table->data.entities = new_entities;

        for (i32 i = 0; i < table->column_count; i++) {
            EcsColumn *column = &table->data.columns[i];
            EcsTypeInfo *ti = column->ti;
            u32 elem_size = ti->size;

            void *new_data = ARENA_ALLOC_ARRAY(world->arena, u8, elem_size * new_size);
            if (column->data) {
                memcpy(new_data, column->data, elem_size * count);
            }
            column->data = new_data;
        }

        table->data.size = new_size;
    }

    table->data.entities[count] = entity;

    for (i32 i = 0; i < table->column_count; i++) {
        EcsColumn *column = &table->data.columns[i];
        EcsTypeInfo *ti = column->ti;
        void *ptr = (u8*)column->data + (ti->size * count);
        memset(ptr, 0, ti->size);
    }

    table->data.count = count + 1;

    EcsRecord *record = ecs_entity_get_record(world, entity);
    if (record) {
        record->table = table;
        record->row = (u32)count;
    }

    return count;
}

void ecs_table_delete(EcsWorld *world, EcsTable *table, i32 row) {
    debug_assert(row >= 0 && row < table->data.count);

    i32 last_row = table->data.count - 1;

    if (row != last_row) {
        table->data.entities[row] = table->data.entities[last_row];

        EcsEntity moved_entity = table->data.entities[row];
        EcsRecord *record = ecs_entity_get_record(world, moved_entity);
        if (record) {
            record->row = (u32)row;
        }

        for (i32 i = 0; i < table->column_count; i++) {
            EcsColumn *column = &table->data.columns[i];
            EcsTypeInfo *ti = column->ti;
            u32 elem_size = ti->size;

            void *dst = (u8*)column->data + (elem_size * row);
            void *src = (u8*)column->data + (elem_size * last_row);
            memcpy(dst, src, elem_size);
        }
    }

    table->data.count--;
}

void* ecs_table_get_column(EcsTable *table, EcsEntity component, i32 *out_column_index) {
    u32 comp_id = ecs_entity_index(component);

    if (comp_id >= ECS_HI_COMPONENT_ID) {
        i32 type_idx = ecs_type_index_of(&table->type, component);
        if (type_idx < 0) {
            if (out_column_index) *out_column_index = -1;
            return NULL;
        }
        if (out_column_index) *out_column_index = -1;
        return NULL;
    }

    i16 res = table->component_map[comp_id];
    if (res <= 0) {
        if (out_column_index) *out_column_index = -1;
        return NULL;
    }

    i32 col_idx = res - 1;
    if (out_column_index) *out_column_index = col_idx;
    return table->data.columns[col_idx].data;
}

void* ecs_table_get_component(EcsTable *table, i32 row, i32 column_index) {
    debug_assert(column_index >= 0 && column_index < table->column_count);
    debug_assert(row >= 0 && row < table->data.count);

    EcsColumn *column = &table->data.columns[column_index];
    return (u8*)column->data + (column->ti->size * row);
}

i32 ecs_table_get_column_index(EcsTable *table, EcsEntity component) {
    u32 comp_id = ecs_entity_index(component);

    if (comp_id >= ECS_HI_COMPONENT_ID) {
        return -1;
    }

    i16 res = table->component_map[comp_id];
    if (res <= 0) {
        return -1;
    }

    return res - 1;
}

b32 ecs_table_has_component(EcsTable *table, EcsEntity component) {
    u32 comp_id = ecs_entity_index(component);

    if (comp_id >= ECS_HI_COMPONENT_ID) {
        return ecs_type_index_of(&table->type, component) >= 0;
    }

    return table->component_map[comp_id] != 0;
}

internal EcsTable* ecs_store_new_table(EcsWorld *world) {
    EcsStore *store = &world->store;

    if (store->table_count >= store->table_cap) {
        i32 new_cap = store->table_cap == 0 ? 16 : store->table_cap * 2;
        EcsTable *new_tables = ARENA_ALLOC_ARRAY(world->arena, EcsTable, new_cap);
        if (store->tables) {
            memcpy(new_tables, store->tables, sizeof(EcsTable) * store->table_count);
        }
        store->tables = new_tables;
        store->table_cap = new_cap;

        if (store->root) {
            store->root = &store->tables[0];
        }
    }

    EcsTable *table = &store->tables[store->table_count];
    memset(table, 0, sizeof(EcsTable));
    table->id = (u64)store->table_count;
    store->table_count++;

    return table;
}

void ecs_store_init(EcsWorld *world) {
    EcsStore *store = &world->store;

    store->table_map = ARENA_ALLOC(world->arena, EcsTableMap);
    ecs_table_map_init(store->table_map, world->arena);

    EcsTable *root = ecs_store_new_table(world);
    ecs_table_init(world, root, NULL);
    store->root = root;
}

EcsTable* ecs_table_find_or_create(EcsWorld *world, const EcsType *type) {
    if (type == NULL || type->count == 0) {
        return world->store.root;
    }

    EcsTable *table = ecs_table_map_get(world->store.table_map, type);
    if (table) {
        return table;
    }

    table = ecs_store_new_table(world);
    ecs_table_init(world, table, type);
    ecs_table_map_set(world->store.table_map, &table->type, table);

    return table;
}
