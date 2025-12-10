#include "ktx.h"
#include "lib/assert.h"
#include "lib/memory.h"
#include "lib/typedefs.h"
#include "os/os.h"

internal const u8 KTX_IDENTIFIER[12] = {0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31,
                                        0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A};

internal const u32 KTX_ENDIANNESS_LE = 0x04030201;
internal const u32 KTX_HEADER_SIZE = 64;

internal u32 read_u32_le(u8 *data) {
  return (u32)data[0] | ((u32)data[1] << 8) | ((u32)data[2] << 16) |
         ((u32)data[3] << 24);
}

internal u32 read_u32_be(u8 *data) {
  return (u32)data[3] | ((u32)data[2] << 8) | ((u32)data[1] << 16) |
         ((u32)data[0] << 24);
}

b32 ktx_parse(u8 *buffer, u32 buffer_len, KTXTexture *out,
              Allocator *allocator) {
  debug_assert(buffer);
  debug_assert(out);
  debug_assert(allocator);

  if (buffer_len < KTX_HEADER_SIZE) {
    LOG_ERROR("KTX buffer too small: % bytes", FMT_UINT(buffer_len));
    return false;
  }

  for (u32 i = 0; i < 12; i++) {
    if (buffer[i] != KTX_IDENTIFIER[i]) {
      LOG_ERROR("Invalid KTX identifier at byte %", FMT_UINT(i));
      return false;
    }
  }

  u32 endianness = read_u32_le(buffer + 12);
  b32 little_endian = (endianness == KTX_ENDIANNESS_LE);

  if (!little_endian && endianness != 0x01020304) {
    LOG_ERROR("Invalid KTX endianness marker: %", FMT_UINT(endianness));
    return false;
  }

  u32 (*read_u32)(u8 *) = little_endian ? read_u32_le : read_u32_be;

  KTXHeader header = {0};
  header.gl_type = read_u32(buffer + 16);
  header.gl_type_size = read_u32(buffer + 20);
  header.gl_format = read_u32(buffer + 24);
  header.gl_internal_format = read_u32(buffer + 28);
  header.gl_base_internal_format = read_u32(buffer + 32);
  header.pixel_width = read_u32(buffer + 36);
  header.pixel_height = read_u32(buffer + 40);
  header.pixel_depth = read_u32(buffer + 44);
  header.num_array_elements = read_u32(buffer + 48);
  header.num_faces = read_u32(buffer + 52);
  header.num_mipmap_levels = read_u32(buffer + 56);
  header.bytes_of_key_value_data = read_u32(buffer + 60);

  if (header.num_mipmap_levels == 0) {
    header.num_mipmap_levels = 1;
  }

  LOG_INFO("KTX: %x%, mipmaps: %, internal_format: 0x%",
           FMT_UINT(header.pixel_width), FMT_UINT(header.pixel_height),
           FMT_UINT(header.num_mipmap_levels),
           FMT_UINT(header.gl_internal_format));

  u32 offset = KTX_HEADER_SIZE + header.bytes_of_key_value_data;

  if (offset > buffer_len) {
    LOG_ERROR("KTX key-value data extends beyond buffer");
    return false;
  }

  KTXMipmap *mipmaps =
      ALLOC_ARRAY(allocator, KTXMipmap, header.num_mipmap_levels);

  u32 width = header.pixel_width;
  u32 height = header.pixel_height;

  for (u32 i = 0; i < header.num_mipmap_levels; i++) {
    if (offset + 4 > buffer_len) {
      LOG_ERROR("KTX mipmap % size extends beyond buffer", FMT_UINT(i));
      return false;
    }

    u32 image_size = read_u32(buffer + offset);
    offset += 4;

    if (offset + image_size > buffer_len) {
      LOG_ERROR("KTX mipmap % data extends beyond buffer", FMT_UINT(i));
      return false;
    }

    mipmaps[i].data = buffer + offset;
    mipmaps[i].size = image_size;
    mipmaps[i].width = width;
    mipmaps[i].height = height;

    offset += image_size;

    u32 padding = (4 - (image_size % 4)) % 4;
    offset += padding;

    width = width > 1 ? width >> 1 : 1;
    height = height > 1 ? height >> 1 : 1;
  }

  out->header = header;
  out->mipmaps = mipmaps;
  out->mipmap_count = header.num_mipmap_levels;

  return true;
}
