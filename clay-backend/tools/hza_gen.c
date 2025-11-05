// hza_gen.c - Tool to generate .hza binary font assets
// Usage: ./hza_gen <input.json> <input.png> <output.hza>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "assert.h"

// Custom headers
#include "typedefs.h"
#include "memory.h"
#include "str.h"
#include "msdf_atlas.h"
#include "json_parser.h"

// Include implementations
#include "memory.c"
#include "json_parser.c"
#include "msdf_atlas.c"

// Read entire file into memory
static u8 *read_file(const char *path, size_t *out_size) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    fprintf(stderr, "Error: Failed to open file '%s'\n", path);
    return NULL;
  }

  // Get file size
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);

  if (size < 0) {
    fprintf(stderr, "Error: Failed to get file size for '%s'\n", path);
    fclose(f);
    return NULL;
  }

  // Allocate buffer
  u8 *buffer = (u8 *)malloc(size + 1); // +1 for null terminator (for JSON)
  if (!buffer) {
    fprintf(stderr, "Error: Failed to allocate %ld bytes for '%s'\n", size,
            path);
    fclose(f);
    return NULL;
  }

  // Read file
  size_t read_size = fread(buffer, 1, size, f);
  if (read_size != (size_t)size) {
    fprintf(stderr, "Error: Failed to read file '%s' (expected %ld, got %zu)\n",
            path, size, read_size);
    free(buffer);
    fclose(f);
    return NULL;
  }

  buffer[size] = '\0'; // Null terminate for JSON strings
  *out_size = size;
  fclose(f);
  return buffer;
}

// Write buffer to file
static b32 write_file(const char *path, const u8 *buffer, size_t size) {
  FILE *f = fopen(path, "wb");
  if (!f) {
    fprintf(stderr, "Error: Failed to create file '%s'\n", path);
    return false;
  }

  size_t written = fwrite(buffer, 1, size, f);
  fclose(f);

  if (written != size) {
    fprintf(stderr, "Error: Failed to write file '%s' (expected %zu, wrote %zu)\n",
            path, size, written);
    return false;
  }

  return true;
}

int main(int argc, char **argv) {
  if (argc != 4) {
    fprintf(stderr, "Usage: %s <input.json> <input.png> <output.hza>\n", argv[0]);
    fprintf(stderr, "\n");
    fprintf(stderr, "Example:\n");
    fprintf(stderr, "  %s Roboto-Regular-atlas.json Roboto-Regular-atlas.png Roboto-Regular.hza\n", argv[0]);
    return 1;
  }

  const char *json_path = argv[1];
  const char *png_path = argv[2];
  const char *output_path = argv[3];

  printf("HZA Font Asset Generator\n");
  printf("=========================\n\n");

  // Read JSON file
  printf("Reading JSON: %s\n", json_path);
  size_t json_size;
  u8 *json_bytes = read_file(json_path, &json_size);
  if (!json_bytes) {
    return 1;
  }
  printf("  Size: %.2f KB\n", BYTES_TO_KB(json_size));

  // Read PNG file
  printf("Reading PNG: %s\n", png_path);
  size_t png_size;
  u8 *png_bytes = read_file(png_path, &png_size);
  if (!png_bytes) {
    free(json_bytes);
    return 1;
  }
  printf("  Size: %.2f KB\n", BYTES_TO_KB(png_size));

  // Setup arena allocator for JSON parsing (1MB should be plenty)
  u8 *arena_buffer = (u8 *)malloc(MB(1));
  if (!arena_buffer) {
    fprintf(stderr, "Error: Failed to allocate arena buffer\n");
    free(json_bytes);
    free(png_bytes);
    return 1;
  }
  ArenaAllocator arena = arena_from_buffer(arena_buffer, MB(1));
  Allocator allocator = make_arena_allocator(&arena);

  // Parse JSON
  printf("\nParsing JSON...\n");
  MsdfAtlasData atlas_data = {0};
  if (!msdf_parse_atlas((const char *)json_bytes, &atlas_data, &allocator)) {
    fprintf(stderr, "Error: Failed to parse MSDF atlas JSON\n");
    free(json_bytes);
    free(png_bytes);
    free(arena_buffer);
    return 1;
  }

  printf("  Atlas: %.0fx%.0f, distanceRange=%.0f, size=%.0f\n",
         atlas_data.atlas.width, atlas_data.atlas.height,
         atlas_data.atlas.distanceRange, atlas_data.atlas.size);
  printf("  Metrics: emSize=%.2f, lineHeight=%.2f, ascender=%.2f, descender=%.2f\n",
         atlas_data.metrics.emSize, atlas_data.metrics.lineHeight,
         atlas_data.metrics.ascender, atlas_data.metrics.descender);
  printf("  Glyphs: %u\n", atlas_data.glyph_count);

  // Calculate memory layout
  printf("\nPacking font asset...\n");
  size_t asset_header_size = sizeof(AssetHeader);
  size_t ui_font_asset_size = sizeof(UIFontAsset);
  size_t glyphs_size = atlas_data.glyph_count * sizeof(MsdfGlyph);
  size_t total_size = asset_header_size + ui_font_asset_size + glyphs_size + png_size;

  printf("  AssetHeader: %zu bytes at offset 0\n", asset_header_size);
  printf("  UIFontAsset: %zu bytes at offset %zu\n", ui_font_asset_size, asset_header_size);
  printf("  Glyphs: %zu bytes at offset %zu\n", glyphs_size, asset_header_size + ui_font_asset_size);
  printf("  PNG data: %zu bytes at offset %zu\n", png_size, asset_header_size + ui_font_asset_size + glyphs_size);
  printf("  Total: %.2f KB\n", BYTES_TO_KB(total_size));

  // Allocate buffer for entire asset
  u8 *asset_buffer = (u8 *)malloc(total_size);
  if (!asset_buffer) {
    fprintf(stderr, "Error: Failed to allocate asset buffer (%zu bytes)\n", total_size);
    free(json_bytes);
    free(png_bytes);
    free(arena_buffer);
    return 1;
  }

  // Fill AssetHeader
  AssetHeader *header = (AssetHeader *)asset_buffer;
  header->version = 1;
  header->asset_size = (u64)total_size;
  header->asset_type_hash = TYPE_HASH(UIFontAsset);

  // Zero dependencies for this asset
  header->dependencies.offset = 0;
  header->dependencies.size = 0;
  header->dependencies.type_size = sizeof(AssetDependency);
  header->dependencies.typehash = TYPE_HASH(AssetDependency);

  // Fill UIFontAsset (after header)
  UIFontAsset *asset = (UIFontAsset *)(asset_buffer + asset_header_size);
  asset->atlas = atlas_data.atlas;
  asset->metrics = atlas_data.metrics;

  // Setup glyphs AssetPtr (offset relative to UIFontAsset base)
  asset->glyphs.offset = (u32)ui_font_asset_size;
  asset->glyphs.size = (u32)glyphs_size;
  asset->glyphs.type_size = sizeof(MsdfGlyph);
  asset->glyphs.typehash = TYPE_HASH(MsdfGlyph);

  // Setup image_data AssetPtr (offset relative to UIFontAsset base)
  asset->image_data.offset = (u32)(ui_font_asset_size + glyphs_size);
  asset->image_data.size = (u32)png_size;
  asset->image_data.type_size = sizeof(u8);
  asset->image_data.typehash = TYPE_HASH(u8);

  // Copy glyph array
  MsdfGlyph *packed_glyphs = assetptr_get(MsdfGlyph, asset, asset->glyphs);
  memcpy(packed_glyphs, atlas_data.glyphs, glyphs_size);

  // Copy PNG data
  u8 *packed_image = assetptr_get(u8, asset, asset->image_data);
  memcpy(packed_image, png_bytes, png_size);

  printf("\nWriting output: %s\n", output_path);
  if (!write_file(output_path, asset_buffer, total_size)) {
    free(json_bytes);
    free(png_bytes);
    free(arena_buffer);
    free(asset_buffer);
    return 1;
  }

  printf("  Success! Wrote %.2f KB\n", BYTES_TO_KB(total_size));

  // Cleanup
  free(json_bytes);
  free(png_bytes);
  free(arena_buffer);
  free(asset_buffer);

  printf("\nDone!\n");
  return 0;
}
