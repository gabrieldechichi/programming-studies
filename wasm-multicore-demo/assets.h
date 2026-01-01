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
#define ASSET_TYPE_BLOB 0

// Declares an asset type - creates compile-time checkable symbol
#define ASSET_TYPE_DECLARE(name) enum { _asset_type_##name##_exists = 1 }

typedef enum {
  ASSET_STATE_NONE = 0,
  ASSET_STATE_LOADING,
  ASSET_STATE_READY,
  ASSET_STATE_FAILED
} AssetState;

typedef struct {
  u8 *buffer;
  u32 len;
  const char *path;
  u32 path_hash;
  AssetTypeId type_id;
} AssetLoadContext;

typedef void (*AssetLoadedCallback)(Handle asset, void *data, void *user_data);
typedef void *(*AssetLoadFn)(AssetLoadContext *ctx);

typedef struct {
  AssetTypeId type_id;
  AssetLoadFn load_fn;
  void *user_data;
} AssetLoader;

typedef struct {
  AssetTypeId type_id;
  AssetState state;
  const char *path;
  u32 path_hash;
  OsFileOp *file_op;
  void *data;
  AssetLoadedCallback callback;
  void *callback_user_data;
} AssetEntry;

HANDLE_ARRAY_DEFINE(AssetEntry);

typedef struct {
  HandleArray_AssetEntry entries;
  FixedArray(AssetLoader, ASSET_MAX_LOADERS) loaders;
  DynArray(Handle) pending_loads;
  Allocator allocator;
} AssetSystem;

void asset_system_init(AssetSystem *s, u32 max_assets);
void _asset_register_loader(AssetSystem *s, AssetTypeId type, AssetLoadFn load, void *user_data);
Handle _asset_load(AssetSystem *s, AssetTypeId type, const char *path, AssetLoadedCallback cb, void *user_data);
Handle asset_load_blob(AssetSystem *s, const char *path, AssetLoadedCallback cb, void *user_data);
void *asset_get(AssetSystem *s, Handle h);
b32 asset_is_ready(AssetSystem *s, Handle h);
void asset_system_update(AssetSystem *s);

// Type-safe macros - validates type_name was declared with ASSET_TYPE_DECLARE
#define asset_register_loader(sys, type_name, load, user_data)                 \
  ((void)_asset_type_##type_name##_exists,                                     \
   _asset_register_loader(sys, ASSET_TYPE(type_name), load, user_data))

#define asset_load(sys, type_name, path, cb, user_data)                        \
  ((void)_asset_type_##type_name##_exists,                                     \
   _asset_load(sys, ASSET_TYPE(type_name), path, cb, user_data))

#endif
