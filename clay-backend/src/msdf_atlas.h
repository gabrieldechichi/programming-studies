#ifndef H_MSDF_ATLAS
#define H_MSDF_ATLAS

#include "typedefs.h"

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

// Complete MSDF atlas data
typedef struct {
  MsdfAtlasConfig atlas;
  MsdfMetrics metrics;
  MsdfGlyph *glyphs;
  u32 glyph_count;
} MsdfAtlasData;

#endif
