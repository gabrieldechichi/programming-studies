#include "assets.h"
#include "animation.h"
#include "game.h"
#include "generated/temp_generated.h"
#include "lib/array.h"
#include "lib/assert.h"
#include "lib/audio.h"
#include "lib/common.h"
#include "lib/fmt.h"
#include "lib/handle.h"
#include "lib/hash.h"
#include "lib/lipsync_algs.h"
#include "lib/memory.h"
#include "lib/string_builder.h"
#include "lib/typedefs.h"
#include "platform/platform.h"
#include "renderer/renderer.h"
#include "stb/stb_image.h"
#include <string.h>

// Forward declarations for asset loaders
internal void *init_texture_asset(GameContext *ctx);
internal void *load_model_asset(u8 *buffer, u32 buffer_len,
                                Allocator *allocator, void *data);
internal void *load_texture_asset(u8 *buffer, u32 buffer_len,
                                  Allocator *allocator, void *data);
internal void *load_animation_asset(u8 *buffer, u32 buffer_len,
                                    Allocator *allocator, void *data);
internal void *load_audio_clip_asset(u8 *buffer, u32 buffer_len,
                                     Allocator *allocator, void *data);
internal void *load_lipsync_profile_asset(u8 *buffer, u32 buffer_len,
                                          Allocator *allocator, void *data);
internal void *load_material_asset(u8 *buffer, u32 buffer_len,
                                   Allocator *allocator, void *data);

internal String64Bytes extract_filename(const char *file_path) {
  const char *last_slash = file_path;
  const char *p = file_path;

  // Find last slash or backslash
  while (*p) {
    if (*p == '/' || *p == '\\') {
      last_slash = p + 1;
    }
    p++;
  }

  return fixedstr64_from_cstr((char *)last_slash);
}

internal bool32 str_ends_with(const char *str, const char *suffix) {
  if (!str || !suffix) {
    return false;
  }

  size_t str_len = strlen(str);
  size_t suffix_len = strlen(suffix);

  if (suffix_len > str_len) {
    return false;
  }

  return strcmp(str + str_len - suffix_len, suffix) == 0;
}

internal bool32 is_webp_texture(AssetType type, const char *file_path) {
  if (type != ASSET_TYPE_TEXTURE) {
    return false;
  }

  return str_ends_with(file_path, ".webp");
}

AssetSystem asset_system_init(Allocator *allocator, u32 max_assets) {
  AssetSystem system = {0};
  assert(allocator);
  assert(max_assets > 0);

  system.entries = ha_init(AssetEntry, allocator, max_assets);

  // Initialize pending loads slice
  system.pending_loads = slice_new_ALLOC(allocator, Handle, max_assets);

  // Initialize loaders slice
  system.loaders = slice_new_ALLOC(allocator, AssetLoaderEntry, 16);

  asset_system_setup_default_loaders(&system);

  return system;
}

void asset_system_register_loader(AssetSystem *system, AssetType type,
                                  const AssetLoader *loader) {
  assert(system);
  assert(loader);

  // Check if loader already registered for this type
  for (u32 i = 0; i < system->loaders.len; i++) {
    debug_assert_msg(system->loaders.items[i].type != type,
                     "Asset loader already registered for type %",
                     FMT_UINT(type));
  }

  AssetLoaderEntry entry = {.type = type, .loader = *loader};

  slice_append(system->loaders, entry);

  LOG_INFO("Registered asset loader for type % (count now %)", FMT_UINT(type),
           FMT_UINT(system->loaders.len));
}

internal AssetLoader *asset_system_find_loader(AssetSystem *system,
                                               AssetType type) {
  for (u32 i = 0; i < system->loaders.len; i++) {
    if (system->loaders.items[i].type == type) {
      return &system->loaders.items[i].loader;
    }
  }
  LOG_WARN("No loader found for asset type %", FMT_UINT(type));
  return NULL;
}

Handle _asset_request(AssetSystem *system, GameContext *ctx, AssetType type,
                      const char *file_path) {
  assert(system);
  assert(file_path);
  assert(type < ASSET_TYPE_COUNT);

  // HACK: change .webp to .png
  if (type == ASSET_TYPE_TEXTURE && str_ends_with(file_path, ".webp")) {
    u32 filepath_len = str_len(file_path);
    char *new_file_path = ALLOC_ARRAY(&ctx->temp_allocator, char, filepath_len);
    memcpy(new_file_path, file_path, filepath_len);
    new_file_path[filepath_len - 4] = 'p';
    new_file_path[filepath_len - 3] = 'n';
    new_file_path[filepath_len - 2] = 'g';
    new_file_path[filepath_len - 1] = 0;
    file_path = new_file_path;
  }

  // HACK: add /assets
  {
    StringBuilder sb;
    u32 size = str_len(file_path) + 20;
    char *buffer = ALLOC_ARRAY(&ctx->temp_allocator, char, size);
    sb_init(&sb, buffer, size);
    sb_append(&sb, "assets/");
    sb_append(&sb, file_path);
    file_path = sb_get(&sb);
  }

  u32 path_hash = fnv1a_hash(file_path);

  ha_foreach_handle(system->entries, h) {
    AssetEntry *entry = ha_get(AssetEntry, &system->entries, h);
    if (entry && entry->file_path_hash == path_hash) {
      LOG_INFO("Already loaded asset for path %, skipping", FMT_STR(file_path));
      return h;
    }
  }

  // Create new asset entry
  AssetEntry entry = {0};
  entry.type = type;
  entry.state = ASSET_STATE_LOADING;
  entry.file_path =
      str_from_cstr_alloc(file_path, str_len(file_path), &ctx->allocator);
  entry.file_path_hash = path_hash;

  // Check if this is a WebP texture - use different loading path
  if (is_webp_texture(type, file_path)) {
    // For WebP: initialize texture data first, then start WebP loading
    AssetLoader *loader = asset_system_find_loader(system, type);
    if (loader && loader->init_fn) {
      entry.data = loader->init_fn(ctx);

      // Get the texture handle for WebP loading
      if (entry.data) {
        Texture *texture = (Texture *)entry.data;
        entry.platform_op = platform_start_webp_texture_load(
            file_path, strlen(file_path), texture->gpu_tex_handle);
        if (entry.platform_op == -1) {
          entry.state = ASSET_STATE_FAILED;
        }
      } else {
        entry.state = ASSET_STATE_FAILED;
      }
    } else {
      entry.state = ASSET_STATE_FAILED;
    }
  } else {
    // Regular file loading path
    entry.platform_op = platform_start_read_file((char *)file_path);
    if (entry.platform_op == -1) {
      entry.state = ASSET_STATE_FAILED;
    }

    // Find and use registered loader for init
    AssetLoader *loader = asset_system_find_loader(system, type);
    if (loader && loader->init_fn) {
      entry.data = loader->init_fn(ctx);
    }
  }

  // Add to handle array
  Handle id = cast_handle(Handle, ha_add(AssetEntry, &system->entries, entry));

  // Add to pending loads slice if loading started successfully
  if (entry.state == ASSET_STATE_LOADING) {
    slice_append(system->pending_loads, id);
  }

  return cast_data(Handle, id);
}

void *_asset_get_data(AssetSystem *system, Handle id) {
  assert(system);

  AssetEntry *entry = ha_get(AssetEntry, &system->entries, id);
  if (!entry || entry->state != ASSET_STATE_READY) {
    return NULL;
  }

  return entry->data;
}

void *_asset_get_data_unsafe(AssetSystem *system, Handle id) {
  assert(system);

  AssetEntry *entry = ha_get(AssetEntry, &system->entries, id);
  if (!entry) {
    return NULL;
  }
  return entry->data;
}

b32 _asset_is_ready(AssetSystem *system, Handle id) {
  assert(system);

  AssetEntry *entry = ha_get(AssetEntry, &system->entries, id);
  return entry && entry->state == ASSET_STATE_READY;
}

void asset_system_update(AssetSystem *system, GameContext *ctx) {
  debug_assert(system);
  if (!system){return;}

  for (i32 i = system->pending_loads.len - 1; i >= 0; i--) {
    Handle handle = system->pending_loads.items[i];
    AssetEntry *entry = ha_get(AssetEntry, &system->entries, handle);

    if (!entry || entry->state != ASSET_STATE_LOADING) {
      slice_remove_swap(system->pending_loads, i);
      continue;
    }

    // Check if this is a WebP texture using different loading path
    bool is_webp = (entry->type == ASSET_TYPE_TEXTURE &&
                    str_ends_with(entry->file_path.value, ".webp"));

    if (is_webp) {
      // Check WebP loading status
      PlatformReadFileState webp_state =
          platform_check_webp_texture_load(entry->platform_op);
      if (webp_state == FREADSTATE_COMPLETED) {
        // WebP was loaded directly to GPU, texture data already set up
        entry->state = ASSET_STATE_READY;
        slice_remove_swap(system->pending_loads, i);
        LOG_INFO("Successfully loaded WebP texture for path %",
                 FMT_STR(entry->file_path.value));
      } else if (webp_state == FREADSTATE_ERROR) {
        LOG_WARN("Failed to load WebP texture for path %",
                 FMT_STR(entry->file_path.value));
        entry->state = ASSET_STATE_FAILED;
        slice_remove_swap(system->pending_loads, i);
      }
    } else {
      // Regular file loading path
      PlatformReadFileState file_state =
          platform_check_read_file(entry->platform_op);
      if (file_state == FREADSTATE_COMPLETED) {
        PlatformFileData file_data = {0};
        if (platform_get_file_data(entry->platform_op, &file_data,
                                   &ctx->temp_allocator)) {
          // Find and use registered loader
          AssetLoader *loader = asset_system_find_loader(system, entry->type);
          void *asset_data = NULL;

          if (loader && loader->load_fn) {
            asset_data = loader->load_fn(file_data.buffer, file_data.buffer_len,
                                         &ctx->allocator, entry->data);
          } else {
            debug_assert_msg(false, "No loader registered for asset type %",
                             FMT_UINT(entry->type));
          }

          if (asset_data) {
            entry->data = asset_data;
            entry->state = ASSET_STATE_READY;
          } else {
            LOG_WARN("Failed to load asset data for path %",
                     FMT_STR(entry->file_path.value));
            entry->state = ASSET_STATE_FAILED;
          }
        } else {
          LOG_WARN("Failed to read asset file for path %",
                   FMT_STR(entry->file_path.value));
          entry->state = ASSET_STATE_FAILED;
        }

        slice_remove_swap(system->pending_loads, i);
        LOG_INFO("Successfully loaded asset for path %",
                 FMT_STR(entry->file_path.value));
      } else if (file_state == FREADSTATE_ERROR) {
        LOG_WARN("Failed to load asset for path %",
                 FMT_STR(entry->file_path.value));
        entry->state = ASSET_STATE_FAILED;
        slice_remove_swap(system->pending_loads, i);
      }
    }
  }
}

void asset_release(AssetSystem *system, Handle id) {
  UNUSED(id);
  assert_msg(false, "We don't have a way to release assets :)");
  assert(system);

  // AssetEntry *entry = AssetEntry_ha_get(&system->entries, id);
  // if (!entry) {
  //   return;
  // }
  //
  // AssetEntry_ha_remove(&system->entries, id);
}

// Asset loader functions
internal void *init_texture_asset(GameContext *ctx) {
  Texture *texture = ALLOC(&ctx->allocator, Texture);
  texture->gpu_tex_handle = renderer_reserve_texture();
  return texture;
}

internal void *load_model_asset(u8 *buffer, u32 buffer_len,
                                Allocator *allocator, void *data) {
  UNUSED(data);
  return read_Model3DData(buffer, buffer_len, allocator);
}

internal void *load_texture_asset(u8 *buffer, u32 buffer_len,
                                  Allocator *allocator, void *data) {
  Texture *texture = (Texture *)data;
  if (!texture) {
    return NULL;
  }

  int x, y, n;
  u8 *decoded_data = stbi_load_from_memory(buffer, buffer_len, &x, &y, &n, 4);

  if (!decoded_data) {
    return NULL;
  }

  texture->image.width = x;
  texture->image.height = y;
  texture->image.byte_len = x * y * 4; // 4 bytes per pixel (RGBA)
  texture->image.data = ALLOC_ARRAY(allocator, u8, texture->image.byte_len);
  memcpy(texture->image.data, decoded_data, texture->image.byte_len);

  b32 success = renderer_set_texture(texture->gpu_tex_handle, &texture->image);
  debug_assert(success);

  return texture;
}

internal void *load_animation_asset(u8 *buffer, u32 buffer_len,
                                    Allocator *allocator, void *data) {
  UNUSED(data);
  u8_Array buffer_data = {.items = buffer, .len = buffer_len};
  return animation_asset_read(buffer_data, allocator);
}

internal void *load_audio_clip_asset(u8 *buffer, u32 buffer_len,
                                     Allocator *allocator, void *data) {
  UNUSED(data);
  WavFile wav = {0};
  if (wav_parse_header(buffer, buffer_len, &wav)) {
    WavFile *wav_asset = ALLOC(allocator, WavFile);
    *wav_asset = wav;
    wav_asset->audio_data = ALLOC_ARRAY(allocator, i16, wav.data_size);
    memcpy_safe(wav_asset->audio_data, wav.audio_data,
                sizeof(i16) * wav.data_size);
    return wav_asset;
  }
  debug_assert_msg(false, "Failed to parse wav header");
  return NULL;
}

internal void *load_lipsync_profile_asset(u8 *buffer, u32 buffer_len,
                                          Allocator *allocator, void *data) {
  UNUSED(data);
  return lipsync_profile_read(buffer, buffer_len, allocator);
}

internal void *load_material_asset(u8 *buffer, u32 buffer_len,
                                   Allocator *allocator, void *data) {
  UNUSED(data);
  return read_MaterialAsset(buffer, buffer_len, allocator);
}

// Setup default asset loaders
void asset_system_setup_default_loaders(AssetSystem *system) {
  // Model loader
  AssetLoader model_loader = {
      .init_fn = NULL,
      .load_fn = load_model_asset,
  };
  asset_system_register_loader(system, ASSET_TYPE_MODEL, &model_loader);

  // Texture loader
  AssetLoader texture_loader = {
      .init_fn = init_texture_asset,
      .load_fn = load_texture_asset,
  };
  asset_system_register_loader(system, ASSET_TYPE_TEXTURE, &texture_loader);

  // Animation loader
  AssetLoader animation_loader = {
      .init_fn = NULL,
      .load_fn = load_animation_asset,
  };
  asset_system_register_loader(system, ASSET_TYPE_ANIMATION, &animation_loader);

  // Audio clip loader
  AssetLoader audio_loader = {
      .init_fn = NULL,
      .load_fn = load_audio_clip_asset,
  };
  asset_system_register_loader(system, ASSET_TYPE_AUDIOCLIP, &audio_loader);

  // LipSync profile loader
  AssetLoader lipsync_loader = {
      .init_fn = NULL,
      .load_fn = load_lipsync_profile_asset,
  };
  asset_system_register_loader(system, ASSET_TYPE_LIPSYNC_PROFILE,
                               &lipsync_loader);

  // // Material loader
  AssetLoader material_loader = {
      .init_fn = NULL,
      .load_fn = load_material_asset,
  };
  asset_system_register_loader(system, ASSET_TYPE_MATERIAL, &material_loader);
}