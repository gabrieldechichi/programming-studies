#ifndef ASSETS_H
#define ASSETS_H

#include "lib/typedefs.h"
#include "lib/handle.h"
#include "lib/memory.h"
#include "lib/hash.h"
#include "lib/array.h"
#include "os/os.h"

#define ASSET_MAX_LOADERS 16

typedef u32 AssetTypeId;
#define ASSET_TYPE(name) (fnv1a_hash(#name))

typedef enum {
  ASSET_STATE_NONE = 0,
  ASSET_STATE_LOADING,
  ASSET_STATE_READY,
  ASSET_STATE_FAILED
} AssetState;

typedef void (*AssetLoadedCallback)(Handle asset, void *data, void *user_data);
typedef void *(*AssetInitFn)(Allocator *alloc, void *user_data);
typedef void *(*AssetLoadFn)(u8 *buffer, u32 len, Allocator *alloc, void *init_data);

typedef struct {
  AssetTypeId type_id;
  AssetInitFn init_fn;
  AssetLoadFn load_fn;
  void *user_data;
} AssetLoader;

typedef struct {
  AssetTypeId type_id;
  AssetState state;
  u32 path_hash;
  OsFileOp *file_op;
  void *data;
  AssetLoadedCallback callback;
  void *callback_user_data;
} AssetEntry;

HANDLE_ARRAY_DEFINE(AssetEntry);

typedef struct {
  HandleArray_AssetEntry entries;
  AssetLoader loaders[ASSET_MAX_LOADERS];
  u32 loader_count;
  Handle_DynArray pending_loads;
  Allocator *allocator;
  TaskSystem *task_system;
} AssetSystem;

void asset_system_init(AssetSystem *s, Allocator *alloc, TaskSystem *tasks, u32 max_assets);
void asset_register_loader(AssetSystem *s, AssetTypeId type, AssetInitFn init, AssetLoadFn load, void *user_data);
Handle asset_load(AssetSystem *s, AssetTypeId type, const char *path, AssetLoadedCallback cb, void *user_data);
void *asset_get(AssetSystem *s, Handle h);
b32 asset_is_ready(AssetSystem *s, Handle h);
void asset_system_update(AssetSystem *s, Allocator *temp_alloc);

#endif
