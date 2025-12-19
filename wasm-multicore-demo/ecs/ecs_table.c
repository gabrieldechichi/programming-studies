#include "ecs_table.h"
#include "lib/assert.h"

internal void ecs_component_record_insert_table(EcsWorld *world, EcsComponentRecord *cr, EcsTable *table, i16 column, i16 type_index) {
    EcsTableRecord *tr = ARENA_ALLOC(world->arena, EcsTableRecord);
    tr->table = table;
    tr->column = column;
    tr->type_index = type_index;
    tr->prev = cr->last;
    tr->next = NULL;

    if (cr->last) {
        cr->last->next = tr;
    } else {
        cr->first = tr;
    }
    cr->last = tr;
    cr->table_count++;
}

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

    table->bloom_filter = 0;
    for (i32 i = 0; i < table->type.count; i++) {
        table->bloom_filter |= ecs_bloom_bit(table->type.array[i]);
    }

    table->column_map = ARENA_ALLOC_ARRAY(world->arena, i16, ECS_HI_COMPONENT_ID);
    memset(table->column_map, 0, sizeof(i16) * ECS_HI_COMPONENT_ID);

    for (i32 i = 0; i < table->type.count; i++) {
        EcsEntity comp = table->type.array[i];
        u32 comp_id = ecs_entity_index(comp);
        if (comp_id < ECS_HI_COMPONENT_ID) {
            table->column_map[comp_id] = (i16)(-(i + 1));
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
                    table->column_map[comp_id] = (i16)(col_idx + 1);
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

    for (i32 i = 0; i < table->type.count; i++) {
        EcsEntity comp = table->type.array[i];
        EcsComponentRecord *cr = ecs_component_record_get(world, comp);
        if (cr) {
            i16 column = ecs_table_get_column_index(table, comp);
            ecs_component_record_insert_table(world, cr, table, column, (i16)i);
        }
    }

    for (i32 i = 0; i < world->cached_query_count; i++) {
        ecs_query_cache_add_table(world->cached_queries[i], table);
    }
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

    i16 res = table->column_map[comp_id];
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

    i16 res = table->column_map[comp_id];
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

    return table->column_map[comp_id] != 0;
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

internal EcsGraphEdge* ecs_graph_edge_get(EcsWorld *world, EcsGraphEdges *edges, EcsEntity id) {
    u32 comp_id = ecs_entity_index(id);

    if (comp_id < ECS_HI_COMPONENT_ID) {
        if (!edges->lo) {
            return NULL;
        }
        EcsGraphEdge *edge = &edges->lo[comp_id];
        if (edge->id == 0) {
            return NULL;
        }
        return edge;
    }

    for (i32 i = 0; i < edges->hi_count; i++) {
        if (edges->hi[i].id == id) {
            return &edges->hi[i];
        }
    }
    return NULL;
}

internal EcsGraphEdge* ecs_graph_edge_ensure(EcsWorld *world, EcsGraphEdges *edges, EcsEntity id) {
    u32 comp_id = ecs_entity_index(id);

    if (comp_id < ECS_HI_COMPONENT_ID) {
        if (!edges->lo) {
            edges->lo = ARENA_ALLOC_ARRAY(world->arena, EcsGraphEdge, ECS_HI_COMPONENT_ID);
            memset(edges->lo, 0, sizeof(EcsGraphEdge) * ECS_HI_COMPONENT_ID);
        }
        return &edges->lo[comp_id];
    }

    for (i32 i = 0; i < edges->hi_count; i++) {
        if (edges->hi[i].id == id) {
            return &edges->hi[i];
        }
    }

    if (edges->hi_count >= edges->hi_cap) {
        i32 new_cap = edges->hi_cap == 0 ? 4 : edges->hi_cap * 2;
        EcsGraphEdge *new_hi = ARENA_ALLOC_ARRAY(world->arena, EcsGraphEdge, new_cap);
        if (edges->hi) {
            memcpy(new_hi, edges->hi, sizeof(EcsGraphEdge) * edges->hi_count);
        }
        edges->hi = new_hi;
        edges->hi_cap = new_cap;
    }

    EcsGraphEdge *edge = &edges->hi[edges->hi_count++];
    memset(edge, 0, sizeof(EcsGraphEdge));
    return edge;
}

internal i32 ecs_type_find_insert(const EcsType *type, EcsEntity to_add) {
    for (i32 i = 0; i < type->count; i++) {
        if (type->array[i] == to_add) {
            return -1;
        }
        if (type->array[i] > to_add) {
            return i;
        }
    }
    return type->count;
}

internal EcsTable* ecs_find_table_with(EcsWorld *world, EcsTable *table, EcsEntity component) {
    i32 insert_pos = ecs_type_find_insert(&table->type, component);
    if (insert_pos < 0) {
        return table;
    }

    i32 new_count = table->type.count + 1;
    EcsEntity *new_array = ARENA_ALLOC_ARRAY(world->arena, EcsEntity, new_count);

    if (insert_pos > 0) {
        memcpy(new_array, table->type.array, sizeof(EcsEntity) * insert_pos);
    }
    new_array[insert_pos] = component;
    if (insert_pos < table->type.count) {
        memcpy(new_array + insert_pos + 1, table->type.array + insert_pos,
               sizeof(EcsEntity) * (table->type.count - insert_pos));
    }

    EcsType new_type = { .array = new_array, .count = new_count };
    return ecs_table_find_or_create(world, &new_type);
}

internal EcsTable* ecs_find_table_without(EcsWorld *world, EcsTable *table, EcsEntity component) {
    i32 remove_pos = ecs_type_index_of(&table->type, component);
    if (remove_pos < 0) {
        return table;
    }

    i32 new_count = table->type.count - 1;
    if (new_count == 0) {
        return world->store.root;
    }

    EcsEntity *new_array = ARENA_ALLOC_ARRAY(world->arena, EcsEntity, new_count);

    if (remove_pos > 0) {
        memcpy(new_array, table->type.array, sizeof(EcsEntity) * remove_pos);
    }
    if (remove_pos < table->type.count - 1) {
        memcpy(new_array + remove_pos, table->type.array + remove_pos + 1,
               sizeof(EcsEntity) * (table->type.count - remove_pos - 1));
    }

    EcsType new_type = { .array = new_array, .count = new_count };
    return ecs_table_find_or_create(world, &new_type);
}

EcsTable* ecs_table_traverse_add(EcsWorld *world, EcsTable *table, EcsEntity component) {
    EcsGraphEdge *edge = ecs_graph_edge_get(world, &table->node.add, component);
    if (edge && edge->to) {
        return edge->to;
    }

    EcsTable *to = ecs_find_table_with(world, table, component);

    edge = ecs_graph_edge_ensure(world, &table->node.add, component);
    edge->id = component;
    edge->to = to;

    return to;
}

EcsTable* ecs_table_traverse_remove(EcsWorld *world, EcsTable *table, EcsEntity component) {
    EcsGraphEdge *edge = ecs_graph_edge_get(world, &table->node.remove, component);
    if (edge && edge->to) {
        return edge->to;
    }

    EcsTable *to = ecs_find_table_without(world, table, component);

    edge = ecs_graph_edge_ensure(world, &table->node.remove, component);
    edge->id = component;
    edge->to = to;

    return to;
}

void ecs_table_move(EcsWorld *world, EcsEntity entity, EcsTable *dst_table, EcsTable *src_table, i32 src_row) {
    i32 dst_row = ecs_table_append(world, dst_table, entity);

    for (i32 src_col = 0; src_col < src_table->column_count; src_col++) {
        EcsColumn *src_column = &src_table->data.columns[src_col];
        EcsEntity comp = src_column->ti->component;

        i32 dst_col = ecs_table_get_column_index(dst_table, comp);
        if (dst_col < 0) {
            continue;
        }

        EcsColumn *dst_column = &dst_table->data.columns[dst_col];
        u32 size = src_column->ti->size;

        void *src_ptr = (u8*)src_column->data + (size * src_row);
        void *dst_ptr = (u8*)dst_column->data + (size * dst_row);
        memcpy(dst_ptr, src_ptr, size);
    }

    ecs_table_delete(world, src_table, src_row);
}

void ecs_add(EcsWorld *world, EcsEntity entity, EcsEntity component) {
    EcsRecord *record = ecs_entity_get_record(world, entity);
    if (!record) {
        return;
    }

    EcsTable *src_table = record->table;
    if (!src_table) {
        src_table = world->store.root;
    }

    EcsTable *dst_table = ecs_table_traverse_add(world, src_table, component);

    if (src_table == dst_table) {
        return;
    }

    if (src_table == world->store.root || src_table->data.count == 0) {
        ecs_table_append(world, dst_table, entity);
    } else {
        i32 src_row = (i32)record->row;
        ecs_table_move(world, entity, dst_table, src_table, src_row);
    }
}

void ecs_remove(EcsWorld *world, EcsEntity entity, EcsEntity component) {
    EcsRecord *record = ecs_entity_get_record(world, entity);
    if (!record || !record->table) {
        return;
    }

    EcsTable *src_table = record->table;
    EcsTable *dst_table = ecs_table_traverse_remove(world, src_table, component);

    if (src_table == dst_table) {
        return;
    }

    i32 src_row = (i32)record->row;
    ecs_table_move(world, entity, dst_table, src_table, src_row);
}

b32 ecs_has(EcsWorld *world, EcsEntity entity, EcsEntity component) {
    EcsRecord *record = ecs_entity_get_record(world, entity);
    if (!record || !record->table) {
        return false;
    }

    u32 comp_id = ecs_entity_index(component);
    if (comp_id < ECS_HI_COMPONENT_ID) {
        return record->table->column_map[comp_id] != 0;
    }

    return ecs_type_index_of(&record->table->type, component) >= 0;
}

void* ecs_get(EcsWorld *world, EcsEntity entity, EcsEntity component) {
    EcsRecord *record = ecs_entity_get_record(world, entity);
    if (!record || !record->table) {
        return NULL;
    }

    i32 col_idx = ecs_table_get_column_index(record->table, component);
    if (col_idx < 0) {
        return NULL;
    }

    return ecs_table_get_component(record->table, (i32)record->row, col_idx);
}

void* ecs_get_mut(EcsWorld *world, EcsEntity entity, EcsEntity component) {
    EcsRecord *record = ecs_entity_get_record(world, entity);
    if (!record) {
        return NULL;
    }

    if (!record->table || !ecs_has(world, entity, component)) {
        ecs_add(world, entity, component);
        record = ecs_entity_get_record(world, entity);
    }

    i32 col_idx = ecs_table_get_column_index(record->table, component);
    if (col_idx < 0) {
        return NULL;
    }

    return ecs_table_get_component(record->table, (i32)record->row, col_idx);
}

void ecs_set_ptr(EcsWorld *world, EcsEntity entity, EcsEntity component, const void *ptr) {
    void *dst = ecs_get_mut(world, entity, component);
    if (!dst) {
        return;
    }

    const EcsTypeInfo *ti = ecs_type_info_get(world, component);
    if (ti) {
        memcpy(dst, ptr, ti->size);
    }
}

void ecs_query_init(EcsQuery *query, EcsWorld *world, EcsEntity *terms, i32 term_count) {
    debug_assert(term_count > 0 && term_count <= ECS_QUERY_MAX_TERMS);

    query->world = world;
    query->term_count = term_count;
    query->field_count = term_count;
    query->is_cached = false;
    query->cache.first = NULL;
    query->cache.last = NULL;
    query->cache.match_count = 0;

    query->bloom_filter = 0;
    query->read_fields = 0;
    query->write_fields = 0;

    for (i32 i = 0; i < term_count; i++) {
        query->terms[i].id = terms[i];
        query->terms[i].oper = EcsOperAnd;
        query->terms[i].inout = EcsInOutDefault;
        query->terms[i].field_index = (i8)i;
        query->terms[i].or_chain_length = 0;
        query->bloom_filter |= ecs_bloom_bit(terms[i]);

        u32 field_bit = 1u << i;
        query->read_fields |= field_bit;
        query->write_fields |= field_bit;
    }
}

void ecs_query_init_terms(EcsQuery *query, EcsWorld *world, EcsTerm *terms, i32 term_count) {
    debug_assert(term_count > 0 && term_count <= ECS_QUERY_MAX_TERMS);

    query->world = world;
    query->term_count = term_count;
    query->is_cached = false;
    query->cache.first = NULL;
    query->cache.last = NULL;
    query->cache.match_count = 0;

    query->bloom_filter = 0;
    query->read_fields = 0;
    query->write_fields = 0;

    i32 field_index = 0;
    for (i32 i = 0; i < term_count; i++) {
        query->terms[i] = terms[i];

        if (terms[i].oper == EcsOperNot) {
            query->terms[i].field_index = -1;
        } else {
            query->terms[i].field_index = (i8)field_index;

            i16 inout = terms[i].inout;
            if (inout == EcsInOutDefault) {
                inout = EcsInOut;
            }

            u32 field_bit = 1u << field_index;
            if (inout != EcsOut && inout != EcsInOutNone) {
                query->read_fields |= field_bit;
            }
            if (inout != EcsIn && inout != EcsInOutNone) {
                query->write_fields |= field_bit;
            }

            field_index++;
        }

        if (terms[i].oper == EcsOperAnd) {
            query->bloom_filter |= ecs_bloom_bit(terms[i].id);
        }
    }
    query->field_count = field_index;
}

internal i32 ecs_query_find_pivot_term(EcsQuery *query) {
    i32 pivot = -1;
    i32 min_tables = INT32_MAX;

    for (i32 i = 0; i < query->term_count; i++) {
        EcsTerm *term = &query->terms[i];
        if (term->oper == EcsOperAnd) {
            EcsComponentRecord *cr = ecs_component_record_get(query->world, term->id);
            i32 table_count = cr ? cr->table_count : 0;
            if (table_count < min_tables) {
                min_tables = table_count;
                pivot = i;
            }
        }
    }

    if (pivot >= 0) {
        return pivot;
    }

    for (i32 i = 0; i < query->term_count; i++) {
        EcsTerm *term = &query->terms[i];
        if (term->oper == EcsOperOr && term->or_chain_length > 0) {
            return i;
        }
    }

    return 0;
}

EcsIter ecs_query_iter(EcsQuery *query) {
    EcsIter it = {0};
    it.world = query->world;
    it.query = query;
    it.table = NULL;
    it.count = 0;
    it.entities = NULL;
    it.cur = NULL;
    it.cache_cur = NULL;
    it.set_fields = 0;

    for (i32 i = 0; i < ECS_QUERY_MAX_TERMS; i++) {
        it.columns[i] = -1;
    }

    return it;
}

internal b32 ecs_iter_next_uncached(EcsIter *it);
internal b32 ecs_iter_next_cached(EcsIter *it);

b32 ecs_iter_next(EcsIter *it) {
    EcsQuery *query = it->query;

    if (query->term_count == 0) {
        return false;
    }

    if (query->is_cached) {
        return ecs_iter_next_cached(it);
    }

    return ecs_iter_next_uncached(it);
}

internal b32 ecs_iter_next_uncached(EcsIter *it) {
    EcsQuery *query = it->query;
    EcsWorld *world = it->world;

    i32 pivot_term = ecs_query_find_pivot_term(query);
    EcsTerm *first_term = &query->terms[pivot_term];

    EcsComponentRecord *cr_first = ecs_component_record_get(world, first_term->id);
    if (!cr_first) {
        return false;
    }

    EcsTableRecord *tr = it->cur ? it->cur->next : cr_first->first;

    while (tr) {
        EcsTable *table = tr->table;

        if (table->data.count == 0) {
            tr = tr->next;
            continue;
        }

        if ((table->bloom_filter & query->bloom_filter) != query->bloom_filter) {
            tr = tr->next;
            continue;
        }

        b32 match = true;
        u32 set_fields = 0;

        for (i32 i = 0; i < ECS_QUERY_MAX_TERMS; i++) {
            it->columns[i] = -1;
        }

        if (first_term->field_index >= 0) {
            it->columns[first_term->field_index] = tr->column;
            set_fields |= (1u << first_term->field_index);
        }

        for (i32 t = 0; t < query->term_count && match; t++) {
            if (t == pivot_term) {
                continue;
            }

            EcsTerm *term = &query->terms[t];
            EcsComponentRecord *cr = ecs_component_record_get(world, term->id);
            b32 has_component = false;
            i16 column = -1;

            if (cr) {
                EcsTableRecord *tr_term = ecs_component_record_get_table(cr, table);
                if (tr_term) {
                    has_component = true;
                    column = tr_term->column;
                }
            }

            switch (term->oper) {
            case EcsOperAnd:
                if (!has_component) {
                    match = false;
                } else {
                    if (term->field_index >= 0) {
                        it->columns[term->field_index] = column;
                        set_fields |= (1u << term->field_index);
                    }
                }
                break;

            case EcsOperNot:
                if (has_component) {
                    match = false;
                }
                break;

            case EcsOperOptional:
                if (has_component && term->field_index >= 0) {
                    it->columns[term->field_index] = column;
                    set_fields |= (1u << term->field_index);
                }
                break;

            case EcsOperOr:
                if (term->or_chain_length > 0) {
                    b32 or_match = has_component;
                    i16 or_column = column;
                    EcsEntity matched_id = term->id;

                    for (i32 o = 1; o < term->or_chain_length && !or_match; o++) {
                        i32 or_idx = t + o;
                        if (or_idx >= query->term_count) break;

                        EcsTerm *or_term = &query->terms[or_idx];
                        EcsComponentRecord *or_cr = ecs_component_record_get(world, or_term->id);
                        if (or_cr) {
                            EcsTableRecord *or_tr = ecs_component_record_get_table(or_cr, table);
                            if (or_tr) {
                                or_match = true;
                                or_column = or_tr->column;
                                matched_id = or_term->id;
                            }
                        }
                    }

                    if (!or_match) {
                        match = false;
                    } else if (term->field_index >= 0) {
                        it->columns[term->field_index] = or_column;
                        set_fields |= (1u << term->field_index);
                    }

                    t += term->or_chain_length - 1;
                }
                break;
            }
        }

        if (match) {
            it->cur = tr;
            it->table = table;
            it->count = table->data.count;
            it->entities = table->data.entities;
            it->set_fields = set_fields;
            return true;
        }

        tr = tr->next;
    }

    return false;
}

void* ecs_iter_field(EcsIter *it, i32 field_index) {
    debug_assert(field_index >= 0 && field_index < it->query->field_count);
    debug_assert(it->table != NULL);

    i16 column = it->columns[field_index];
    if (column < 0) {
        return NULL;
    }

    return it->table->data.columns[column].data;
}

i32 ecs_iter_field_column(EcsIter *it, i32 field_index) {
    debug_assert(field_index >= 0 && field_index < it->query->field_count);
    return it->columns[field_index];
}

b32 ecs_query_table_matches(EcsQuery *query, EcsTable *table, i16 *out_columns, u32 *out_set_fields) {
    EcsWorld *world = query->world;

    if ((table->bloom_filter & query->bloom_filter) != query->bloom_filter) {
        return false;
    }

    for (i32 i = 0; i < ECS_QUERY_MAX_TERMS; i++) {
        out_columns[i] = -1;
    }
    *out_set_fields = 0;

    for (i32 t = 0; t < query->term_count; t++) {
        EcsTerm *term = &query->terms[t];
        EcsComponentRecord *cr = ecs_component_record_get(world, term->id);
        b32 has_component = false;
        i16 column = -1;

        if (cr) {
            EcsTableRecord *tr_term = ecs_component_record_get_table(cr, table);
            if (tr_term) {
                has_component = true;
                column = tr_term->column;
            }
        }

        switch (term->oper) {
        case EcsOperAnd:
            if (!has_component) {
                return false;
            }
            if (term->field_index >= 0) {
                out_columns[term->field_index] = column;
                *out_set_fields |= (1u << term->field_index);
            }
            break;

        case EcsOperNot:
            if (has_component) {
                return false;
            }
            break;

        case EcsOperOptional:
            if (has_component && term->field_index >= 0) {
                out_columns[term->field_index] = column;
                *out_set_fields |= (1u << term->field_index);
            }
            break;

        case EcsOperOr:
            if (term->or_chain_length > 0) {
                b32 or_match = has_component;
                i16 or_column = column;

                for (i32 o = 1; o < term->or_chain_length && !or_match; o++) {
                    i32 or_idx = t + o;
                    if (or_idx >= query->term_count) break;

                    EcsTerm *or_term = &query->terms[or_idx];
                    EcsComponentRecord *or_cr = ecs_component_record_get(world, or_term->id);
                    if (or_cr) {
                        EcsTableRecord *or_tr = ecs_component_record_get_table(or_cr, table);
                        if (or_tr) {
                            or_match = true;
                            or_column = or_tr->column;
                        }
                    }
                }

                if (!or_match) {
                    return false;
                }
                if (term->field_index >= 0) {
                    out_columns[term->field_index] = or_column;
                    *out_set_fields |= (1u << term->field_index);
                }

                t += term->or_chain_length - 1;
            }
            break;
        }
    }

    return true;
}

void ecs_query_cache_init(EcsQuery *query) {
    EcsWorld *world = query->world;

    query->cache.first = NULL;
    query->cache.last = NULL;
    query->cache.match_count = 0;
    query->is_cached = true;

    if (world->cached_query_count >= world->cached_query_cap) {
        i32 new_cap = world->cached_query_cap == 0 ? 16 : world->cached_query_cap * 2;
        EcsQuery **new_queries = ARENA_ALLOC_ARRAY(world->arena, EcsQuery*, new_cap);
        if (world->cached_queries) {
            memcpy(new_queries, world->cached_queries, sizeof(EcsQuery*) * world->cached_query_count);
        }
        world->cached_queries = new_queries;
        world->cached_query_cap = new_cap;
    }

    world->cached_queries[world->cached_query_count++] = query;

    ecs_query_cache_populate(query);
}

void ecs_query_cache_add_table(EcsQuery *query, EcsTable *table) {
    i16 columns[ECS_QUERY_MAX_TERMS];
    u32 set_fields;

    if (!ecs_query_table_matches(query, table, columns, &set_fields)) {
        return;
    }

    EcsWorld *world = query->world;
    EcsQueryCacheMatch *match = ARENA_ALLOC(world->arena, EcsQueryCacheMatch);
    match->table = table;
    memcpy(match->columns, columns, sizeof(columns));
    match->set_fields = set_fields;
    match->next = NULL;

    if (query->cache.last) {
        query->cache.last->next = match;
    } else {
        query->cache.first = match;
    }
    query->cache.last = match;
    query->cache.match_count++;
}

void ecs_query_cache_remove_table(EcsQuery *query, EcsTable *table) {
    EcsQueryCacheMatch *prev = NULL;
    EcsQueryCacheMatch *cur = query->cache.first;

    while (cur) {
        if (cur->table == table) {
            if (prev) {
                prev->next = cur->next;
            } else {
                query->cache.first = cur->next;
            }

            if (cur == query->cache.last) {
                query->cache.last = prev;
            }

            query->cache.match_count--;
            return;
        }
        prev = cur;
        cur = cur->next;
    }
}

void ecs_query_cache_populate(EcsQuery *query) {
    EcsWorld *world = query->world;

    query->cache.first = NULL;
    query->cache.last = NULL;
    query->cache.match_count = 0;

    for (i32 i = 0; i < world->store.table_count; i++) {
        EcsTable *table = &world->store.tables[i];
        if (table->data.count == 0) {
            continue;
        }
        ecs_query_cache_add_table(query, table);
    }
}

internal b32 ecs_iter_next_cached(EcsIter *it) {
    EcsQueryCacheMatch *match = it->cache_cur ? it->cache_cur->next : it->query->cache.first;

    while (match) {
        if (match->table->data.count > 0) {
            it->cache_cur = match;
            it->table = match->table;
            it->count = match->table->data.count;
            it->entities = match->table->data.entities;
            memcpy(it->columns, match->columns, sizeof(match->columns));
            it->set_fields = match->set_fields;
            return true;
        }
        match = match->next;
    }

    return false;
}
