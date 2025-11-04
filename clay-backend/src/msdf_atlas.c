#ifndef H_MSDF_ATLAS_C
#define H_MSDF_ATLAS_C

#include "msdf_atlas.h"
#include "json_parser.h"
#include "memory.h"
#include "str.h"

// Parse atlas configuration
static b32 parse_atlas_config(JsonParser *parser, MsdfAtlasConfig *config) {
  if (!json_expect_object_start(parser)) return false;

  json_skip_whitespace(parser);
  while (json_peek_char(parser) != '}') {
    char *key = json_expect_key(parser, NULL);
    if (!key) return false;
    if (!json_expect_colon(parser)) return false;

    if (strcmp(key, "type") == 0) {
      json_parse_string_value(parser);
    } else if (strcmp(key, "distanceRange") == 0) {
      config->distanceRange = (f32)json_parse_number_value(parser);
    } else if (strcmp(key, "distanceRangeMiddle") == 0) {
      config->distanceRangeMiddle = (f32)json_parse_number_value(parser);
    } else if (strcmp(key, "size") == 0) {
      config->size = (f32)json_parse_number_value(parser);
    } else if (strcmp(key, "width") == 0) {
      config->width = (f32)json_parse_number_value(parser);
    } else if (strcmp(key, "height") == 0) {
      config->height = (f32)json_parse_number_value(parser);
    } else if (strcmp(key, "yOrigin") == 0) {
      json_parse_string_value(parser);
    }

    json_skip_whitespace(parser);
    if (json_peek_char(parser) == ',') {
      json_expect_comma(parser);
      json_skip_whitespace(parser);
    }
  }

  return json_expect_object_end(parser);
}

// Parse font metrics
static b32 parse_metrics(JsonParser *parser, MsdfMetrics *metrics) {
  if (!json_expect_object_start(parser)) return false;

  json_skip_whitespace(parser);
  while (json_peek_char(parser) != '}') {
    char *key = json_expect_key(parser, NULL);
    if (!key) return false;
    if (!json_expect_colon(parser)) return false;

    if (strcmp(key, "emSize") == 0) {
      metrics->emSize = (f32)json_parse_number_value(parser);
    } else if (strcmp(key, "lineHeight") == 0) {
      metrics->lineHeight = (f32)json_parse_number_value(parser);
    } else if (strcmp(key, "ascender") == 0) {
      metrics->ascender = (f32)json_parse_number_value(parser);
    } else if (strcmp(key, "descender") == 0) {
      metrics->descender = (f32)json_parse_number_value(parser);
    } else if (strcmp(key, "underlineY") == 0) {
      metrics->underlineY = (f32)json_parse_number_value(parser);
    } else if (strcmp(key, "underlineThickness") == 0) {
      metrics->underlineThickness = (f32)json_parse_number_value(parser);
    }

    json_skip_whitespace(parser);
    if (json_peek_char(parser) == ',') {
      json_expect_comma(parser);
      json_skip_whitespace(parser);
    }
  }

  return json_expect_object_end(parser);
}

// Parse plane bounds
static b32 parse_plane_bounds(JsonParser *parser, MsdfPlaneBounds *bounds) {
  if (!json_expect_object_start(parser)) return false;

  json_skip_whitespace(parser);
  while (json_peek_char(parser) != '}') {
    char *key = json_expect_key(parser, NULL);
    if (!key) return false;
    if (!json_expect_colon(parser)) return false;

    if (strcmp(key, "left") == 0) {
      bounds->left = (f32)json_parse_number_value(parser);
    } else if (strcmp(key, "bottom") == 0) {
      bounds->bottom = (f32)json_parse_number_value(parser);
    } else if (strcmp(key, "right") == 0) {
      bounds->right = (f32)json_parse_number_value(parser);
    } else if (strcmp(key, "top") == 0) {
      bounds->top = (f32)json_parse_number_value(parser);
    }

    json_skip_whitespace(parser);
    if (json_peek_char(parser) == ',') {
      json_expect_comma(parser);
      json_skip_whitespace(parser);
    }
  }

  return json_expect_object_end(parser);
}

// Parse atlas bounds
static b32 parse_atlas_bounds(JsonParser *parser, MsdfAtlasBounds *bounds) {
  if (!json_expect_object_start(parser)) return false;

  json_skip_whitespace(parser);
  while (json_peek_char(parser) != '}') {
    char *key = json_expect_key(parser, NULL);
    if (!key) return false;
    if (!json_expect_colon(parser)) return false;

    if (strcmp(key, "left") == 0) {
      bounds->left = (f32)json_parse_number_value(parser);
    } else if (strcmp(key, "bottom") == 0) {
      bounds->bottom = (f32)json_parse_number_value(parser);
    } else if (strcmp(key, "right") == 0) {
      bounds->right = (f32)json_parse_number_value(parser);
    } else if (strcmp(key, "top") == 0) {
      bounds->top = (f32)json_parse_number_value(parser);
    }

    json_skip_whitespace(parser);
    if (json_peek_char(parser) == ',') {
      json_expect_comma(parser);
      json_skip_whitespace(parser);
    }
  }

  return json_expect_object_end(parser);
}

// Parse single glyph
static b32 parse_glyph(JsonParser *parser, MsdfGlyph *glyph) {
  glyph->has_visual = false;

  if (!json_expect_object_start(parser)) return false;

  json_skip_whitespace(parser);
  while (json_peek_char(parser) != '}') {
    char *key = json_expect_key(parser, NULL);
    if (!key) return false;
    if (!json_expect_colon(parser)) return false;

    if (strcmp(key, "unicode") == 0) {
      glyph->unicode = (u32)json_parse_number_value(parser);
    } else if (strcmp(key, "advance") == 0) {
      glyph->advance = (f32)json_parse_number_value(parser);
    } else if (strcmp(key, "planeBounds") == 0) {
      glyph->has_visual = true;
      if (!parse_plane_bounds(parser, &glyph->planeBounds)) return false;
    } else if (strcmp(key, "atlasBounds") == 0) {
      if (!parse_atlas_bounds(parser, &glyph->atlasBounds)) return false;
    }

    json_skip_whitespace(parser);
    if (json_peek_char(parser) == ',') {
      json_expect_comma(parser);
      json_skip_whitespace(parser);
    }
  }

  return json_expect_object_end(parser);
}

// Parse glyphs array
static b32 parse_glyphs(JsonParser *parser, MsdfGlyph **glyphs, u32 *glyph_count, Allocator *allocator) {
  if (!json_expect_char(parser, '[')) return false;

  // Count glyphs first
  u32 count = 0;
  u32 saved_pos = parser->pos;

  json_skip_whitespace(parser);
  while (json_peek_char(parser) != ']') {
    // Skip this glyph object
    i32 depth = 0;
    b32 in_string = false;
    while (parser->pos < parser->len) {
      char c = json_peek_char(parser);
      if (c == '"' && (parser->pos == 0 || parser->str[parser->pos - 1] != '\\')) {
        in_string = !in_string;
      }
      json_consume_char(parser);

      if (!in_string) {
        if (c == '{') depth++;
        else if (c == '}') {
          depth--;
          if (depth == 0) break;
        }
      }
    }
    count++;

    json_skip_whitespace(parser);
    if (json_peek_char(parser) == ',') {
      json_expect_comma(parser);
      json_skip_whitespace(parser);
    }
  }

  // Allocate array
  *glyph_count = count;
  *glyphs = ALLOC_ARRAY(allocator, MsdfGlyph, count);

  // Reset and parse glyphs
  parser->pos = saved_pos;
  json_skip_whitespace(parser);

  for (u32 i = 0; i < count; i++) {
    if (!parse_glyph(parser, &(*glyphs)[i])) return false;

    json_skip_whitespace(parser);
    if (json_peek_char(parser) == ',') {
      json_expect_comma(parser);
      json_skip_whitespace(parser);
    }
  }

  return json_expect_char(parser, ']');
}

// Parse complete MSDF atlas JSON
b32 msdf_parse_atlas(const char *json_str, MsdfAtlasData *atlas, Allocator *allocator) {
  JsonParser parser = json_parser_init(json_str, allocator);

  if (!json_expect_object_start(&parser)) return false;

  json_skip_whitespace(&parser);
  while (json_peek_char(&parser) != '}') {
    char *key = json_expect_key(&parser, NULL);
    if (!key) return false;
    if (!json_expect_colon(&parser)) return false;

    if (strcmp(key, "atlas") == 0) {
      if (!parse_atlas_config(&parser, &atlas->atlas)) return false;
    } else if (strcmp(key, "metrics") == 0) {
      if (!parse_metrics(&parser, &atlas->metrics)) return false;
    } else if (strcmp(key, "glyphs") == 0) {
      if (!parse_glyphs(&parser, &atlas->glyphs, &atlas->glyph_count, allocator)) return false;
    } else if (strcmp(key, "kerning") == 0) {
      // Skip empty kerning array
      if (!json_expect_char(&parser, '[')) return false;
      json_skip_whitespace(&parser);
      if (!json_expect_char(&parser, ']')) return false;
    }

    json_skip_whitespace(&parser);
    if (json_peek_char(&parser) == ',') {
      json_expect_comma(&parser);
      json_skip_whitespace(&parser);
    }
  }

  return json_expect_object_end(&parser);
}

#endif
