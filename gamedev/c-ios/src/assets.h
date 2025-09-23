#ifndef H_ASSETS
#define H_ASSETS

#include "context.h"
#include "lib/handle.h"
#include "lib/memory.h"
#include "lib/string.h"
#include "lib/typedefs.h"
#include "platform/platform.h"

// Typed handles for each asset type
TYPED_HANDLE_DEFINE(Model3DData);
TYPED_HANDLE_DEFINE(AnimationAsset);
TYPED_HANDLE_DEFINE(WavFile);
TYPED_HANDLE_DEFINE(LipSyncProfile);
TYPED_HANDLE_DEFINE(MaterialAsset);

typedef enum {
  ASSET_TYPE_MODEL,
  ASSET_TYPE_TEXTURE,
  ASSET_TYPE_ANIMATION,
  ASSET_TYPE_AUDIOCLIP,
  ASSET_TYPE_LIPSYNC_PROFILE,
  ASSET_TYPE_MATERIAL,
  ASSET_TYPE_COUNT
} AssetType;

// Forward declare for function pointers
struct AssetEntry;

// Asset loader interface
typedef struct {
  void *(*init_fn)(GameContext *ctx);
  void *(*load_fn)(u8 *buffer, u32 buffer_len, Allocator *allocator,
                   void *data);
} AssetLoader;

// Asset loader registry entry
typedef struct {
  AssetType type;
  AssetLoader loader;
} AssetLoaderEntry;

slice_define(AssetLoaderEntry);

typedef enum {
  ASSET_STATE_UNLOADED,
  ASSET_STATE_LOADING,
  ASSET_STATE_READY,
  ASSET_STATE_FAILED
} AssetState;

// Asset entry
typedef struct {
  AssetType type;
  AssetState state;
  String file_path;
  u32 file_path_hash;
  void *data;
  PlatformReadFileOp platform_op;
} AssetEntry;

TYPED_HANDLE_DEFINE(AssetEntry);
HANDLE_ARRAY_DEFINE(AssetEntry);

typedef struct {
  HandleArray_AssetEntry entries;
  Handle_Slice pending_loads;
  AssetLoaderEntry_Slice loaders;
} AssetSystem;

AssetSystem asset_system_init(Allocator *allocator, u32 max_assets);

void asset_system_register_loader(AssetSystem *system, AssetType type,
                                  const AssetLoader *loader);

void asset_system_setup_default_loaders(AssetSystem *system);

Handle _asset_request(AssetSystem *system, GameContext *ctx, AssetType type,
                      const char *file_path);

void *_asset_get_data(AssetSystem *system, Handle id);

// returns data even if the asset hasn't loaded yet
void *_asset_get_data_unsafe(AssetSystem *system, Handle id);

b32 _asset_is_ready(AssetSystem *system, Handle id);

void asset_system_update(AssetSystem *system, GameContext *ctx);

void asset_release(AssetSystem *system, Handle id);

#define asset_is_ready(assets, h)                                              \
  _asset_is_ready(assets, cast_handle(Handle, h))

#define asset_request(type, system, ctx, file_path)                            \
  _Generic(type,                                                               \
      Model3DData: cast_handle(                                                \
               Model3DData_Handle,                                             \
               _asset_request(system, ctx, ASSET_TYPE_MODEL, file_path)),      \
      Texture: cast_handle(                                                    \
               Texture_Handle,                                                 \
               _asset_request(system, ctx, ASSET_TYPE_TEXTURE, file_path)),    \
      AnimationAsset: cast_handle(                                             \
               AnimationAsset_Handle,                                          \
               _asset_request(system, ctx, ASSET_TYPE_ANIMATION, file_path)),  \
      WavFile: cast_handle(                                                    \
               WavFile_Handle,                                                 \
               _asset_request(system, ctx, ASSET_TYPE_AUDIOCLIP, file_path)),  \
      LipSyncProfile: cast_handle(LipSyncProfile_Handle,                       \
                                  _asset_request(system, ctx,                  \
                                                 ASSET_TYPE_LIPSYNC_PROFILE,   \
                                                 file_path)),                  \
      MaterialAsset: cast_handle(                                              \
               MaterialAsset_Handle,                                           \
               _asset_request(system, ctx, ASSET_TYPE_MATERIAL, file_path)),   \
      default: _asset_request(system, ctx, -1, file_path))

#define asset_get_data(type, system, handle)                                   \
  ({                                                                           \
    is_type(type##_Handle *, &handle);                                         \
    ((type *)_asset_get_data(system, cast_handle(Handle, handle)));            \
  })

#define asset_get_data_unsafe(type, system, handle)                            \
  ({                                                                           \
    is_type(type##_Handle *, &handle);                                         \
    ((type *)_asset_get_data_unsafe(system, cast_handle(Handle, handle)));     \
  })

u32 asset_system_pending_count(AssetSystem *system) {
  debug_assert(system);
  if (!system) {
    return 0;
  }
  return system->pending_loads.len;
}

#endif // H_ASSETS