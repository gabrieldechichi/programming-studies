/*
 MSDF Atlas Generator
 Generates a multi-channel signed distance field atlas from a TTF/OTF font

 Usage:
   ./atlas_gen font.ttf output_prefix [atlas_size] [glyph_size]

 Example:
   ./atlas_gen font/OpenSans-Regular.ttf output 512 32

 Output:
   output_msdf.png - MSDF atlas texture
   output.json     - Glyph metadata (positions, UVs, metrics)
*/

#include "msdf.h"
#include "atlas_packer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// Unity build for json_serializer and dependencies
#include "../../src/typedefs.h"
#include "../../src/assert.h"
#include "../../src/memory.h"
#include "../../src/str.h"
#include "../../src/json_serializer.h"

// Include implementation files
#include "../../src/memory.c"
#include "../../src/json_serializer.c"

#define DEFAULT_ATLAS_SIZE 512
#define DEFAULT_GLYPH_SIZE 32
#define GLYPH_PADDING 2  // Padding around each glyph to prevent bleeding

// First printable ASCII char
#define CHAR_START 32
// Last printable ASCII char
#define CHAR_END 126
#define CHAR_COUNT (CHAR_END - CHAR_START + 1)

typedef struct {
    int codepoint;
    int x, y, w, h;           // Position in atlas (pixels)
    float u0, v0, u1, v1;     // UV coordinates (normalized 0-1)
    int advance;              // Horizontal advance
    int bearing_x, bearing_y; // Bearing
    int width, height;        // Glyph dimensions
} AtlasGlyph;

static uint8_t* read_file(const char* path, size_t* size) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        printf("Error: Could not open file '%s'\n", path);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    rewind(file);

    uint8_t* buffer = malloc(*size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    fread(buffer, *size, 1, file);
    buffer[*size] = '\0';
    fclose(file);

    return buffer;
}

static void write_json_metadata(const char* output_path, AtlasGlyph* glyphs, int glyph_count,
                                  int atlas_width, int atlas_height, int glyph_size) {
    // Create arena allocator for JSON serialization (1MB should be plenty)
    uint8_t* json_buffer = malloc(MB(1));
    if (!json_buffer) {
        printf("Error: Failed to allocate JSON buffer\n");
        return;
    }

    ArenaAllocator arena = arena_from_buffer(json_buffer, MB(1));
    Allocator allocator = make_arena_allocator(&arena);

    // Initialize JSON serializer
    JsonSerializer serializer = json_serializer_init(&allocator, MB(1));

    // Start root object
    write_object_start(&serializer);

    // Write atlas metadata
    write_key(&serializer, "atlas_width");
    serialize_number_value(&serializer, atlas_width);
    write_comma(&serializer);

    write_key(&serializer, "atlas_height");
    serialize_number_value(&serializer, atlas_height);
    write_comma(&serializer);

    write_key(&serializer, "glyph_size");
    serialize_number_value(&serializer, glyph_size);
    write_comma(&serializer);

    write_key(&serializer, "padding");
    serialize_number_value(&serializer, GLYPH_PADDING);
    write_comma(&serializer);

    // Write glyphs array
    write_key(&serializer, "glyphs");
    write_array_start(&serializer);

    for (int i = 0; i < glyph_count; i++) {
        AtlasGlyph* g = &glyphs[i];

        write_object_start(&serializer);

        // Char field (single character string)
        write_key(&serializer, "char");
        char char_str[2] = {g->codepoint >= 32 && g->codepoint <= 126 ? g->codepoint : '?', '\0'};
        serialize_string_value(&serializer, char_str);
        write_comma(&serializer);

        write_key(&serializer, "codepoint");
        serialize_number_value(&serializer, g->codepoint);
        write_comma(&serializer);

        write_key(&serializer, "x");
        serialize_number_value(&serializer, g->x);
        write_comma(&serializer);

        write_key(&serializer, "y");
        serialize_number_value(&serializer, g->y);
        write_comma(&serializer);

        write_key(&serializer, "w");
        serialize_number_value(&serializer, g->w);
        write_comma(&serializer);

        write_key(&serializer, "h");
        serialize_number_value(&serializer, g->h);
        write_comma(&serializer);

        write_key(&serializer, "u0");
        serialize_number_value(&serializer, g->u0);
        write_comma(&serializer);

        write_key(&serializer, "v0");
        serialize_number_value(&serializer, g->v0);
        write_comma(&serializer);

        write_key(&serializer, "u1");
        serialize_number_value(&serializer, g->u1);
        write_comma(&serializer);

        write_key(&serializer, "v1");
        serialize_number_value(&serializer, g->v1);
        write_comma(&serializer);

        write_key(&serializer, "advance");
        serialize_number_value(&serializer, g->advance);
        write_comma(&serializer);

        write_key(&serializer, "bearing_x");
        serialize_number_value(&serializer, g->bearing_x);
        write_comma(&serializer);

        write_key(&serializer, "bearing_y");
        serialize_number_value(&serializer, g->bearing_y);
        write_comma(&serializer);

        write_key(&serializer, "width");
        serialize_number_value(&serializer, g->width);
        write_comma(&serializer);

        write_key(&serializer, "height");
        serialize_number_value(&serializer, g->height);

        write_object_end(&serializer);

        if (i < glyph_count - 1) {
            write_comma(&serializer);
        }
    }

    write_array_end(&serializer);
    write_object_end(&serializer);

    // Finalize JSON
    char* json_str = json_serializer_finalize(&serializer);

    // Write to file
    FILE* f = fopen(output_path, "w");
    if (!f) {
        printf("Error: Could not write metadata to '%s'\n", output_path);
        free(json_buffer);
        return;
    }

    fprintf(f, "%s", json_str);
    fclose(f);

    // Cleanup
    free(json_buffer);
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("MSDF Atlas Generator\n");
        printf("Usage: %s font.ttf output_prefix [atlas_size] [glyph_size]\n", argv[0]);
        printf("\n");
        printf("Arguments:\n");
        printf("  font.ttf       - Path to TTF/OTF font file\n");
        printf("  output_prefix  - Output file prefix (generates PREFIX_msdf.png and PREFIX.json)\n");
        printf("  atlas_size     - Atlas texture size (default: %d)\n", DEFAULT_ATLAS_SIZE);
        printf("  glyph_size     - MSDF resolution per glyph (default: %d)\n", DEFAULT_GLYPH_SIZE);
        printf("\n");
        printf("Example:\n");
        printf("  %s font/OpenSans-Regular.ttf output 512 32\n", argv[0]);
        return 0;
    }

    const char* font_path = argv[1];
    const char* output_prefix = argv[2];
    int atlas_size = argc > 3 ? atoi(argv[3]) : DEFAULT_ATLAS_SIZE;
    int glyph_size = argc > 4 ? atoi(argv[4]) : DEFAULT_GLYPH_SIZE;

    printf("MSDF Atlas Generator\n");
    printf("===================\n");
    printf("Font:        %s\n", font_path);
    printf("Output:      %s_msdf.png, %s.json\n", output_prefix, output_prefix);
    printf("Atlas size:  %dx%d\n", atlas_size, atlas_size);
    printf("Glyph size:  %dx%d\n", glyph_size, glyph_size);
    printf("Padding:     %d pixels\n", GLYPH_PADDING);
    printf("Characters:  %d ('%c' to '%c')\n\n", CHAR_COUNT, CHAR_START, CHAR_END);

    // Load font file
    size_t font_data_size;
    uint8_t* font_data = read_file(font_path, &font_data_size);
    if (!font_data) {
        return 1;
    }

    // Initialize stb_truetype
    stbtt_fontinfo font;
    if (!stbtt_InitFont(&font, font_data, stbtt_GetFontOffsetForIndex(font_data, 0))) {
        printf("Error: Failed to initialize font\n");
        free(font_data);
        return 1;
    }

    // Get font metrics
    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &line_gap);
    float scale = stbtt_ScaleForPixelHeight(&font, glyph_size);

    printf("Font metrics (at size %d):\n", glyph_size);
    printf("  Ascent:    %.1f\n", ascent * scale);
    printf("  Descent:   %.1f\n", descent * scale);
    printf("  Line gap:  %.1f\n\n", line_gap * scale);

    // Create atlas texture (RGB)
    int atlas_channels = 3; // RGB for MSDF
    uint8_t* atlas_data = calloc(atlas_size * atlas_size * atlas_channels, 1);
    if (!atlas_data) {
        printf("Error: Failed to allocate atlas memory\n");
        free(font_data);
        return 1;
    }

    // Create atlas packer
    Atlas* packer = atlas_create(atlas_size, atlas_size, 256);
    if (!packer) {
        printf("Error: Failed to create atlas packer\n");
        free(atlas_data);
        free(font_data);
        return 1;
    }

    // Array to store glyph metadata
    AtlasGlyph* glyphs = malloc(sizeof(AtlasGlyph) * CHAR_COUNT);
    int glyph_count = 0;

    // Generate MSDF for each character
    printf("Generating MSDF glyphs...\n");
    for (int i = 0; i < CHAR_COUNT; i++) {
        int codepoint = CHAR_START + i;

        // Generate MSDF
        ex_metrics_t metrics;
        float* msdf = ex_msdf_glyph(&font, codepoint, glyph_size, glyph_size, &metrics, 1);

        if (!msdf) {
            // Handle space and other whitespace characters that have no outline
            // We still need their metrics for proper text layout
            if (codepoint == 32) { // Space character
                int glyph_index = stbtt_FindGlyphIndex(&font, codepoint);
                if (glyph_index != 0) {
                    // Get advance width for space
                    int advance_width, left_side_bearing;
                    stbtt_GetGlyphHMetrics(&font, glyph_index, &advance_width, &left_side_bearing);

                    // Add space glyph with metrics but no atlas space
                    AtlasGlyph* g = &glyphs[glyph_count++];
                    g->codepoint = codepoint;
                    g->x = 0;
                    g->y = 0;
                    g->w = 0;
                    g->h = 0;
                    g->u0 = 0;
                    g->v0 = 0;
                    g->u1 = 0;
                    g->v1 = 0;
                    g->advance = (int)(advance_width * scale);
                    g->bearing_x = (int)(left_side_bearing * scale);
                    g->bearing_y = 0;
                    g->width = 0;
                    g->height = 0;

                    printf("  Added space character (no MSDF, advance=%d)\n", g->advance);
                } else {
                    printf("  Warning: Space character glyph not found in font\n");
                }
            } else {
                printf("  Warning: Failed to generate MSDF for character '%c' (%d)\n", codepoint, codepoint);
            }
            continue;
        }

        // Pack into atlas with padding
        int glyph_with_padding = glyph_size + GLYPH_PADDING * 2;
        int atlas_x, atlas_y;
        if (!atlas_add_rect(packer, glyph_with_padding, glyph_with_padding, &atlas_x, &atlas_y)) {
            printf("Error: Atlas is full! Increase atlas_size or reduce glyph_size\n");
            free(msdf);
            break;
        }

        // Copy MSDF data to atlas (with padding offset)
        for (int y = 0; y < glyph_size; y++) {
            for (int x = 0; x < glyph_size; x++) {
                int src_idx = (y * glyph_size + x) * 3;
                int dst_x = atlas_x + GLYPH_PADDING + x;
                int dst_y = atlas_y + GLYPH_PADDING + y;
                int dst_idx = (dst_y * atlas_size + dst_x) * 3;

                // Convert float distance to uint8 [0, 255]
                // msdf-c outputs: dist/RANGE + 0.5 where RANGE=1.0
                // This gives: inside < 0.5, edge = 0.5, outside > 0.5
                // Standard MSDF expects: inside > 0.5, edge = 0.5, outside < 0.5
                // So invert: 1.0 - value
                atlas_data[dst_idx + 0] = (uint8_t)(255.0f * (1.0f - msdf[src_idx + 0]));
                atlas_data[dst_idx + 1] = (uint8_t)(255.0f * (1.0f - msdf[src_idx + 1]));
                atlas_data[dst_idx + 2] = (uint8_t)(255.0f * (1.0f - msdf[src_idx + 2]));
            }
        }

        // Store glyph metadata
        AtlasGlyph* g = &glyphs[glyph_count++];
        g->codepoint = codepoint;
        g->x = atlas_x;
        g->y = atlas_y;
        g->w = glyph_with_padding;
        g->h = glyph_with_padding;
        g->u0 = (float)atlas_x / atlas_size;
        g->v0 = (float)atlas_y / atlas_size;
        g->u1 = (float)(atlas_x + glyph_with_padding) / atlas_size;
        g->v1 = (float)(atlas_y + glyph_with_padding) / atlas_size;
        g->advance = metrics.advance;
        g->bearing_x = metrics.left_bearing;
        g->bearing_y = metrics.iy0;
        g->width = metrics.ix1 - metrics.ix0;
        g->height = metrics.iy1 - metrics.iy0;

        free(msdf);

        // Show progress every 10 glyphs
        if ((i + 1) % 10 == 0 || i == CHAR_COUNT - 1) {
            printf("  Progress: %d/%d glyphs\n", glyph_count, CHAR_COUNT);
        }
    }

    printf("\nGenerated %d glyphs\n", glyph_count);

    // Write atlas PNG
    char atlas_path[512];
    snprintf(atlas_path, sizeof(atlas_path), "%s_msdf.png", output_prefix);
    if (!stbi_write_png(atlas_path, atlas_size, atlas_size, atlas_channels, atlas_data, atlas_size * atlas_channels)) {
        printf("Error: Failed to write atlas PNG to '%s'\n", atlas_path);
    } else {
        printf("Wrote atlas: %s\n", atlas_path);
    }

    // Write metadata JSON
    char json_path[512];
    snprintf(json_path, sizeof(json_path), "%s.json", output_prefix);
    write_json_metadata(json_path, glyphs, glyph_count, atlas_size, atlas_size, glyph_size);
    printf("Wrote metadata: %s\n", json_path);

    // Cleanup
    atlas_delete(packer);
    free(atlas_data);
    free(glyphs);
    free(font_data);

    printf("\nDone!\n");
    return 0;
}
