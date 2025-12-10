#ifndef H_DDS
#define H_DDS

#include "lib/common.h"
#include "lib/memory.h"
#include "lib/typedefs.h"

typedef enum {
  DXGI_FORMAT_UNKNOWN = 0,
  DXGI_FORMAT_R32G32B32A32_FLOAT = 2,
  DXGI_FORMAT_R16G16B16A16_FLOAT = 10,
  DXGI_FORMAT_R10G10B10A2_UNORM = 24,
  DXGI_FORMAT_R8G8B8A8_UNORM = 28,
  DXGI_FORMAT_R8G8B8A8_UNORM_SRGB = 29,
  DXGI_FORMAT_BC1_UNORM = 71,
  DXGI_FORMAT_BC1_UNORM_SRGB = 72,
  DXGI_FORMAT_BC2_UNORM = 74,
  DXGI_FORMAT_BC2_UNORM_SRGB = 75,
  DXGI_FORMAT_BC3_UNORM = 77,
  DXGI_FORMAT_BC3_UNORM_SRGB = 78,
  DXGI_FORMAT_BC4_UNORM = 80,
  DXGI_FORMAT_BC4_SNORM = 81,
  DXGI_FORMAT_BC5_UNORM = 83,
  DXGI_FORMAT_BC5_SNORM = 84,
  DXGI_FORMAT_BC6H_UF16 = 95,
  DXGI_FORMAT_BC6H_SF16 = 96,
  DXGI_FORMAT_BC7_UNORM = 98,
  DXGI_FORMAT_BC7_UNORM_SRGB = 99,
} DXGI_FORMAT;

typedef struct {
  u32 size;
  u32 flags;
  u32 fourCC;
  u32 rgb_bit_count;
  u32 r_bit_mask;
  u32 g_bit_mask;
  u32 b_bit_mask;
  u32 a_bit_mask;
} DDS_PIXELFORMAT;

typedef struct {
  u32 size;
  u32 flags;
  u32 height;
  u32 width;
  u32 pitch_or_linear_size;
  u32 depth;
  u32 mipmap_count;
  u32 reserved1[11];
  DDS_PIXELFORMAT ddspf;
  u32 caps;
  u32 caps2;
  u32 caps3;
  u32 caps4;
  u32 reserved2;
} DDS_HEADER;

typedef struct {
  DXGI_FORMAT dxgi_format;
  u32 resource_dimension;
  u32 misc_flag;
  u32 array_size;
  u32 misc_flags2;
} DDS_HEADER_DXT10;

typedef struct {
  u8 *data;
  u32 size;
  u32 width;
  u32 height;
} DDSMipmap;

typedef struct {
  DDS_HEADER header;
  DDS_HEADER_DXT10 header_dxt10;
  b32 has_dxt10_header;
  DXGI_FORMAT format;
  DDSMipmap *mipmaps;
  u32 mipmap_count;
} DDSTexture;

b32 dds_parse(u8 *buffer, u32 buffer_len, DDSTexture *out,
              Allocator *allocator);

#endif
