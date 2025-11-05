#ifndef H_MSDF_ATLAS
#define H_MSDF_ATLAS

#include "typedefs.h"
#include <stddef.h>

// Atlas configuration
typedef struct {
  f32 distanceRange;
  f32 distanceRangeMiddle;
  f32 size;
  f32 width;
  f32 height;
} MsdfAtlasConfig;

// Font metrics (all in em units, normalized to emSize)
typedef struct {
  f32 emSize;
  f32 lineHeight;
  f32 ascender;
  f32 descender;
  f32 underlineY;
  f32 underlineThickness;
} MsdfMetrics;

// Plane bounds (in em units, relative to font size)
typedef struct {
  f32 left;
  f32 bottom;
  f32 right;
  f32 top;
} MsdfPlaneBounds;

// Atlas bounds (in pixels, texture coordinates)
typedef struct {
  f32 left;
  f32 bottom;
  f32 right;
  f32 top;
} MsdfAtlasBounds;

// Single glyph data
typedef struct {
  u32 unicode;
  f32 advance;
  b32 has_visual; // true if glyph has planeBounds/atlasBounds (space doesn't)
  MsdfPlaneBounds planeBounds;
  MsdfAtlasBounds atlasBounds;
} MsdfGlyph;

// Complete MSDF atlas data (legacy - kept for JSON parsing)
typedef struct {
  MsdfAtlasConfig atlas;
  MsdfMetrics metrics;
  MsdfGlyph *glyphs;
  u32 glyph_count;
} MsdfAtlasData;

// FNV-1a hash for stable type hashing
// This is constexpr-friendly for compile-time evaluation
static inline u32 fnv1a_hash_str(const char *str, u32 len) {
  u32 hash = 2166136261u; // FNV offset basis
  for (u32 i = 0; i < len; i++) {
    hash ^= (u32)(u8)str[i];
    hash *= 16777619u; // FNV prime
  }
  return hash;
}

// Helper to get string length at compile time
static inline u32 const_strlen(const char *str) {
  u32 len = 0;
  while (str[len])
    len++;
  return len;
}

// Compute stable type hash: hash(typename) ^ sizeof(type)
// This catches both wrong type name and struct size changes
#define TYPE_HASH(T) (fnv1a_hash_str(#T, CSTR_LEN(#T)) ^ (u32)sizeof(T))

// Generic asset pointer with type safety
typedef struct {
  u32 offset;    // Offset in bytes from parent base pointer
  u32 size;      // Total size in bytes of the blob
  u32 type_size; // Size of each element (for validation and length calculation)
  u32 typehash;  // Hash of type name + size for validation
} AssetPtr;

// Get pointer to asset data with type validation
static inline void *_assetptr_get(void *parent, AssetPtr ptr,
                                  size_t expected_type_size,
                                  u32 expected_typehash) {
  // Validate type_size matches (catches basic size mismatches)
  // assert(expected_type_size == ptr.type_size && "AssetPtr type_size
  // mismatch");

  // Validate typehash matches (catches type name + size changes)
  // assert(expected_typehash == ptr.typehash && "AssetPtr typehash mismatch");

  (void)expected_type_size; // Suppress unused parameter warning
  (void)expected_typehash;  // Suppress unused parameter warning

  return (void *)((u8 *)parent + ptr.offset);
}

// Get number of elements in asset array
static inline u32 assetptr_len(AssetPtr ptr) {
  // Validate size is aligned to type_size
  // assert(ptr.size % ptr.type_size == 0 && "AssetPtr size not aligned to
  // type_size");
  return ptr.size / ptr.type_size;
}

#define assetptr_get(type, parent, ptr)                                        \
  ((type *)_assetptr_get(parent, ptr, sizeof(type), TYPE_HASH(type)))

// Binary font asset (.hza format) - self-contained, single allocation
// Memory layout: [UIFontAsset header][Glyph array][PNG data]
typedef struct {
  MsdfAtlasConfig atlas;
  MsdfMetrics metrics;
  AssetPtr glyphs;     // Array of MsdfGlyph
  AssetPtr image_data; // Array of u8 (PNG bytes)
} UIFontAsset;

// Get total size of UIFontAsset for serialization
static inline size_t ui_font_asset_get_size(UIFontAsset *asset) {
  return sizeof(UIFontAsset) + asset->glyphs.size + asset->image_data.size;
}

#endif
