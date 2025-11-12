// Compressed texture format support detection

export interface CompressedFormatInfo {
  name: string;
  extension: any | null;
  formats: number[];
  fileExtension: string;
}

export interface SupportedFormats {
  s3tc: CompressedFormatInfo;
  s3tc_srgb: CompressedFormatInfo;
  etc: CompressedFormatInfo;
  etc1: CompressedFormatInfo;
  astc: CompressedFormatInfo;
  pvrtc: CompressedFormatInfo;
}

export function detectCompressedFormats(gl: WebGL2RenderingContext): SupportedFormats {
  // S3TC / DXT (Desktop, some mobile)
  const s3tc = gl.getExtension('WEBGL_compressed_texture_s3tc');
  const s3tc_srgb = gl.getExtension('WEBGL_compressed_texture_s3tc_srgb');

  // ETC (Mobile, WebGL2 baseline)
  const etc = gl.getExtension('WEBGL_compressed_texture_etc');
  const etc1 = gl.getExtension('WEBGL_compressed_texture_etc1');

  // ASTC (Modern mobile)
  const astc = gl.getExtension('WEBGL_compressed_texture_astc');

  // PVRTC (iOS)
  const pvrtc = gl.getExtension('WEBGL_compressed_texture_pvrtc');

  // Build S3TC formats - include both regular and sRGB if available
  const s3tcFormats: number[] = [];
  if (s3tc) {
    s3tcFormats.push(
      s3tc.COMPRESSED_RGB_S3TC_DXT1_EXT,
      s3tc.COMPRESSED_RGBA_S3TC_DXT1_EXT,
      s3tc.COMPRESSED_RGBA_S3TC_DXT3_EXT,
      s3tc.COMPRESSED_RGBA_S3TC_DXT5_EXT,
    );
  }
  if (s3tc_srgb) {
    s3tcFormats.push(
      s3tc_srgb.COMPRESSED_SRGB_S3TC_DXT1_EXT,
      s3tc_srgb.COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT,
      s3tc_srgb.COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT,
      s3tc_srgb.COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT,
    );
  }

  return {
    s3tc: {
      name: 'S3TC (DXT)',
      extension: s3tc || s3tc_srgb,
      formats: s3tcFormats,
      fileExtension: 'dxt5',
    },
    s3tc_srgb: {
      name: 'S3TC sRGB',
      extension: s3tc_srgb,
      formats: s3tc_srgb ? [
        s3tc_srgb.COMPRESSED_SRGB_S3TC_DXT1_EXT,
        s3tc_srgb.COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT,
        s3tc_srgb.COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT,
        s3tc_srgb.COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT,
      ] : [],
      fileExtension: 'dxt5_srgb',
    },
    etc: {
      name: 'ETC2/EAC',
      extension: etc,
      formats: etc ? [
        etc.COMPRESSED_RGB8_ETC2,
        etc.COMPRESSED_RGBA8_ETC2_EAC,
        etc.COMPRESSED_SRGB8_ETC2,
        etc.COMPRESSED_SRGB8_ALPHA8_ETC2_EAC,
      ] : [],
      fileExtension: 'etc2',
    },
    etc1: {
      name: 'ETC1',
      extension: etc1,
      formats: etc1 ? [
        etc1.COMPRESSED_RGB_ETC1_WEBGL,
      ] : [],
      fileExtension: 'etc1',
    },
    astc: {
      name: 'ASTC',
      extension: astc,
      formats: astc ? [
        astc.COMPRESSED_RGBA_ASTC_4x4_KHR,
        astc.COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR,
      ] : [],
      fileExtension: 'astc',
    },
    pvrtc: {
      name: 'PVRTC',
      extension: pvrtc,
      formats: pvrtc ? [
        pvrtc.COMPRESSED_RGB_PVRTC_4BPPV1_IMG,
        pvrtc.COMPRESSED_RGBA_PVRTC_4BPPV1_IMG,
      ] : [],
      fileExtension: 'pvrtc',
    },
  };
}

export function getPreferredFormat(formats: SupportedFormats): CompressedFormatInfo | null {
  // Priority order: S3TC (desktop) > ETC2 (mobile baseline) > ASTC > PVRTC > ETC1
  if (formats.s3tc.extension) return formats.s3tc;
  if (formats.etc.extension) return formats.etc;
  if (formats.astc.extension) return formats.astc;
  if (formats.pvrtc.extension) return formats.pvrtc;
  if (formats.etc1.extension) return formats.etc1;

  return null;
}

export function getSupportedFormatsInPriorityOrder(formats: SupportedFormats): CompressedFormatInfo[] {
  // Return all supported formats in priority order
  const priorityOrder = [
    formats.s3tc,
    formats.etc,
    formats.astc,
    formats.pvrtc,
    formats.etc1,
    formats.s3tc_srgb,
  ];

  return priorityOrder.filter(format => format.extension !== null);
}

export function formatSupportsInternalFormat(format: CompressedFormatInfo, internalFormat: number): boolean {
  return format.formats.includes(internalFormat);
}
