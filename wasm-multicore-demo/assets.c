#include "assets.h"
#include "context.h"
#include "lib/assert.h"
#include "lib/thread_context.h"

void asset_system_init(AssetSystem *s,
                       u32 max_assets) {
  debug_assert(s);
  debug_assert(max_assets > 0);

  AppContext *app_ctx = app_ctx_current();
  Allocator alloc = make_arena_allocator(&app_ctx->arena);
  s->allocator = alloc;
  s->loaders.len = 0;
  s->entries = ha_init(AssetEntry, &s->allocator, max_assets);
  s->pending_loads = dyn_arr_new_alloc(&s->allocator, Handle, max_assets);
}

internal AssetLoader *asset_find_loader(AssetSystem *s, AssetTypeId type_id) {
  for (u32 i = 0; i < s->loaders.len; i++) {
    if (s->loaders.items[i].type_id == type_id) {
      return &s->loaders.items[i];
    }
  }
  return NULL;
}

void _asset_register_loader(AssetSystem *s, AssetTypeId type_id,
                            AssetLoadFn load, void *user_data) {
  debug_assert(s);
  debug_assert(load);

  AssetLoader *existing = asset_find_loader(s, type_id);
  debug_assert_msg(!existing, "Loader already registered for type %",
                   FMT_UINT(type_id));

  AssetLoader loader = {
      .type_id = type_id,
      .load_fn = load,
      .user_data = user_data,
  };
  fixed_arr_append(s->loaders, loader);
}

Handle _asset_load(AssetSystem *s, AssetTypeId type_id, const char *path,
                   AssetLoadedCallback cb, void *user_data) {
  debug_assert(s);
  debug_assert(path);

  u32 path_hash = fnv1a_hash(path);

  ha_foreach_handle(s->entries, h) {
    AssetEntry *entry = ha_get(AssetEntry, &s->entries, h);
    if (entry && entry->path_hash == path_hash && entry->type_id == type_id) {
      if (entry->state == ASSET_STATE_READY && cb) {
        cb(h, entry->data, user_data);
      }
      return h;
    }
  }

  AssetLoader *loader = asset_find_loader(s, type_id);
  if (!loader) {
    LOG_ERROR("No loader registered for asset type %", FMT_UINT(type_id));
    return INVALID_HANDLE;
  }

  AssetEntry entry = {0};
  entry.type_id = type_id;
  entry.state = ASSET_STATE_LOADING;
  entry.path = path;
  entry.path_hash = path_hash;
  entry.callback = cb;
  entry.callback_user_data = user_data;

  entry.file_op = os_start_read_file(path);
  if (!entry.file_op) {
    LOG_ERROR("Failed to start loading asset: %", FMT_STR(path));
    entry.state = ASSET_STATE_FAILED;
  }

  Handle handle = ha_add(AssetEntry, &s->entries, entry);

  if (entry.state == ASSET_STATE_LOADING) {
    dyn_arr_append(s->pending_loads, handle);
  }

  return handle;
}

Handle asset_load_blob(AssetSystem *s, const char *path, AssetLoadedCallback cb,
                       void *user_data) {
  debug_assert(s);
  debug_assert(path);

  u32 path_hash = fnv1a_hash(path);

  ha_foreach_handle(s->entries, h) {
    AssetEntry *entry = ha_get(AssetEntry, &s->entries, h);
    if (entry && entry->path_hash == path_hash &&
        entry->type_id == ASSET_TYPE_BLOB) {
      if (entry->state == ASSET_STATE_READY && cb) {
        cb(h, entry->data, user_data);
      }
      return h;
    }
  }

  AssetEntry entry = {0};
  entry.type_id = ASSET_TYPE_BLOB;
  entry.state = ASSET_STATE_LOADING;
  entry.path = path;
  entry.path_hash = path_hash;
  entry.callback = cb;
  entry.callback_user_data = user_data;

  entry.file_op = os_start_read_file(path);
  if (!entry.file_op) {
    LOG_ERROR("Failed to start loading blob asset: %", FMT_STR(path));
    entry.state = ASSET_STATE_FAILED;
  }

  Handle handle = ha_add(AssetEntry, &s->entries, entry);

  if (entry.state == ASSET_STATE_LOADING) {
    dyn_arr_append(s->pending_loads, handle);
  }

  return handle;
}

void *asset_get(AssetSystem *s, Handle h) {
  debug_assert(s);

  AssetEntry *entry = ha_get(AssetEntry, &s->entries, h);
  if (!entry || entry->state != ASSET_STATE_READY) {
    return NULL;
  }
  return entry->data;
}

b32 asset_is_ready(AssetSystem *s, Handle h) {
  debug_assert(s);

  AssetEntry *entry = ha_get(AssetEntry, &s->entries, h);
  return entry && entry->state == ASSET_STATE_READY;
}

void asset_system_update(AssetSystem *s) {
  debug_assert(s);

  // We don't expect many assets to poll each frame, so we only run on main
  // thread to avoid synchronization overhead
  if (!is_main_thread()) {
    return;
  }

  ThreadContext *tctx = tctx_current();
  Allocator temp_alloc = make_arena_allocator(&tctx->temp_arena);

  for (i32 i = (i32)s->pending_loads.len - 1; i >= 0; i--) {
    Handle handle = s->pending_loads.items[i];
    AssetEntry *entry = ha_get(AssetEntry, &s->entries, handle);

    if (!entry || entry->state != ASSET_STATE_LOADING) {
      dyn_arr_remove_swap(s->pending_loads, i);
      continue;
    }

    OsFileReadState file_state = os_check_read_file(entry->file_op);

    if (file_state == OS_FILE_READ_STATE_COMPLETED) {
      PlatformFileData file_data = {0};

      if (os_get_file_data(entry->file_op, &file_data, &temp_alloc)) {
        void *asset_data = NULL;

        if (entry->type_id == ASSET_TYPE_BLOB) {
          asset_data = file_data.buffer;
        } else {
          AssetLoader *loader = asset_find_loader(s, entry->type_id);
          if (loader && loader->load_fn) {
            AssetLoadContext ctx = {
                .buffer = file_data.buffer,
                .len = file_data.buffer_len,
                .path = entry->path,
                .path_hash = entry->path_hash,
                .type_id = entry->type_id,
            };
            asset_data = loader->load_fn(&ctx);
          } else {
            LOG_ERROR("No loader found for asset type %",
                      FMT_UINT(entry->type_id));
          }
        }

        if (asset_data) {
          entry->data = asset_data;
          entry->state = ASSET_STATE_READY;

          if (entry->callback) {
            entry->callback(handle, entry->data, entry->callback_user_data);
          }
        } else {
          LOG_ERROR("Loader failed for asset type %", FMT_UINT(entry->type_id));
          entry->state = ASSET_STATE_FAILED;
        }
      } else {
        LOG_ERROR("Failed to get file data for asset");
        entry->state = ASSET_STATE_FAILED;
      }

      dyn_arr_remove_swap(s->pending_loads, i);

    } else if (file_state == OS_FILE_READ_STATE_ERROR) {
      LOG_ERROR("File read error for asset");
      entry->state = ASSET_STATE_FAILED;
      dyn_arr_remove_swap(s->pending_loads, i);
    }
  }
}
