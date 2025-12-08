#ifndef H_KTX
#define H_KTX

#include "lib/common.h"
#include "lib/memory.h"
#include "lib/typedefs.h"

typedef struct {
  u32 gl_type;
  u32 gl_type_size;
  u32 gl_format;
  u32 gl_internal_format;
  u32 gl_base_internal_format;
  u32 pixel_width;
  u32 pixel_height;
  u32 pixel_depth;
  u32 num_array_elements;
  u32 num_faces;
  u32 num_mipmap_levels;
  u32 bytes_of_key_value_data;
} KTXHeader;

typedef struct {
  u8 *data;
  u32 size;
  u32 width;
  u32 height;
} KTXMipmap;

typedef struct {
  KTXHeader header;
  KTXMipmap *mipmaps;
  u32 mipmap_count;
} KTXTexture;

b32 ktx_parse(u8 *buffer, u32 buffer_len, KTXTexture *out,
              Allocator *allocator);

#endif
