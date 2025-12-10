#include "dds.h"
#include "lib/assert.h"
#include "lib/memory.h"
#include "lib/typedefs.h"
#include "os/os.h"

internal const u32 DDS_MAGIC = 0x20534444;
internal const u32 DDS_HEADER_SIZE = 124;
internal const u32 DDS_HEADER_DXT10_SIZE = 20;

internal const u32 DDSD_MIPMAPCOUNT = 0x20000;
internal const u32 DDPF_FOURCC = 0x4;
internal const u32 DX10_FOURCC = 0x30315844;

internal u32 read_u32_le_dds(u8 *data) {
  return (u32)data[0] | ((u32)data[1] << 8) | ((u32)data[2] << 16) |
         ((u32)data[3] << 24);
}

internal u32 calculate_mipmap_size(u32 width, u32 height, DXGI_FORMAT format) {
  u32 block_size = 0;
  u32 block_width = 1;
  u32 block_height = 1;

  switch (format) {
  case DXGI_FORMAT_BC1_UNORM:
  case DXGI_FORMAT_BC1_UNORM_SRGB:
  case DXGI_FORMAT_BC4_UNORM:
  case DXGI_FORMAT_BC4_SNORM:
    block_size = 8;
    block_width = 4;
    block_height = 4;
    break;

  case DXGI_FORMAT_BC2_UNORM:
  case DXGI_FORMAT_BC2_UNORM_SRGB:
  case DXGI_FORMAT_BC3_UNORM:
  case DXGI_FORMAT_BC3_UNORM_SRGB:
  case DXGI_FORMAT_BC5_UNORM:
  case DXGI_FORMAT_BC5_SNORM:
  case DXGI_FORMAT_BC6H_UF16:
  case DXGI_FORMAT_BC6H_SF16:
  case DXGI_FORMAT_BC7_UNORM:
  case DXGI_FORMAT_BC7_UNORM_SRGB:
    block_size = 16;
    block_width = 4;
    block_height = 4;
    break;

  case DXGI_FORMAT_R8G8B8A8_UNORM:
  case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    return width * height * 4;

  case DXGI_FORMAT_R16G16B16A16_FLOAT:
    return width * height * 8;

  case DXGI_FORMAT_R32G32B32A32_FLOAT:
    return width * height * 16;

  default:
    return 0;
  }

  u32 blocks_x = (width + block_width - 1) / block_width;
  u32 blocks_y = (height + block_height - 1) / block_height;
  return blocks_x * blocks_y * block_size;
}

b32 dds_parse(u8 *buffer, u32 buffer_len, DDSTexture *out,
              Allocator *allocator) {
  debug_assert(buffer);
  debug_assert(out);
  debug_assert(allocator);

  if (buffer_len < 4 + DDS_HEADER_SIZE) {
    LOG_ERROR("DDS buffer too small: % bytes", FMT_UINT(buffer_len));
    return false;
  }

  u32 magic = read_u32_le_dds(buffer);
  if (magic != DDS_MAGIC) {
    LOG_ERROR("Invalid DDS magic: 0x%", FMT_UINT(magic));
    return false;
  }

  u32 offset = 4;

  DDS_HEADER header = {0};
  header.size = read_u32_le_dds(buffer + offset + 0);
  header.flags = read_u32_le_dds(buffer + offset + 4);
  header.height = read_u32_le_dds(buffer + offset + 8);
  header.width = read_u32_le_dds(buffer + offset + 12);
  header.pitch_or_linear_size = read_u32_le_dds(buffer + offset + 16);
  header.depth = read_u32_le_dds(buffer + offset + 20);
  header.mipmap_count = read_u32_le_dds(buffer + offset + 24);

  offset += 28 + 44;

  header.ddspf.size = read_u32_le_dds(buffer + offset + 0);
  header.ddspf.flags = read_u32_le_dds(buffer + offset + 4);
  header.ddspf.fourCC = read_u32_le_dds(buffer + offset + 8);
  header.ddspf.rgb_bit_count = read_u32_le_dds(buffer + offset + 12);
  header.ddspf.r_bit_mask = read_u32_le_dds(buffer + offset + 16);
  header.ddspf.g_bit_mask = read_u32_le_dds(buffer + offset + 20);
  header.ddspf.b_bit_mask = read_u32_le_dds(buffer + offset + 24);
  header.ddspf.a_bit_mask = read_u32_le_dds(buffer + offset + 28);

  offset += 32;

  header.caps = read_u32_le_dds(buffer + offset + 0);
  header.caps2 = read_u32_le_dds(buffer + offset + 4);
  header.caps3 = read_u32_le_dds(buffer + offset + 8);
  header.caps4 = read_u32_le_dds(buffer + offset + 12);
  header.reserved2 = read_u32_le_dds(buffer + offset + 16);

  offset += 20;

  if (!(header.flags & DDSD_MIPMAPCOUNT) || header.mipmap_count == 0) {
    header.mipmap_count = 1;
  }

  b32 has_dxt10 = (header.ddspf.flags & DDPF_FOURCC) &&
                  (header.ddspf.fourCC == DX10_FOURCC);

  DDS_HEADER_DXT10 header_dxt10 = {0};
  DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

  if (has_dxt10) {
    if (buffer_len < offset + DDS_HEADER_DXT10_SIZE) {
      LOG_ERROR("DDS DXT10 header extends beyond buffer");
      return false;
    }

    header_dxt10.dxgi_format = read_u32_le_dds(buffer + offset + 0);
    header_dxt10.resource_dimension = read_u32_le_dds(buffer + offset + 4);
    header_dxt10.misc_flag = read_u32_le_dds(buffer + offset + 8);
    header_dxt10.array_size = read_u32_le_dds(buffer + offset + 12);
    header_dxt10.misc_flags2 = read_u32_le_dds(buffer + offset + 16);

    offset += DDS_HEADER_DXT10_SIZE;
    format = (DXGI_FORMAT)header_dxt10.dxgi_format;
  } else {
    LOG_ERROR("Non-DX10 DDS formats not yet supported");
    return false;
  }

  LOG_INFO("DDS: %x%, mipmaps: %, format: %", FMT_UINT(header.width),
           FMT_UINT(header.height), FMT_UINT(header.mipmap_count),
           FMT_UINT(format));

  DDSMipmap *mipmaps = ALLOC_ARRAY(allocator, DDSMipmap, header.mipmap_count);

  u32 width = header.width;
  u32 height = header.height;

  for (u32 i = 0; i < header.mipmap_count; i++) {
    u32 mip_size = calculate_mipmap_size(width, height, format);

    if (mip_size == 0) {
      LOG_ERROR("Unsupported DDS format: %", FMT_UINT(format));
      return false;
    }

    if (offset + mip_size > buffer_len) {
      LOG_ERROR("DDS mipmap % data extends beyond buffer", FMT_UINT(i));
      return false;
    }

    mipmaps[i].data = buffer + offset;
    mipmaps[i].size = mip_size;
    mipmaps[i].width = width;
    mipmaps[i].height = height;

    offset += mip_size;

    width = width > 1 ? width >> 1 : 1;
    height = height > 1 ? height >> 1 : 1;
  }

  out->header = header;
  out->header_dxt10 = header_dxt10;
  out->has_dxt10_header = has_dxt10;
  out->format = format;
  out->mipmaps = mipmaps;
  out->mipmap_count = header.mipmap_count;

  return true;
}
