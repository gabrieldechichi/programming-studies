import { parseKTX, type KTXTextureData } from './ktx-parser';
import { type CompressedFormatInfo, formatSupportsInternalFormat } from './format-detection';

export async function loadKTXFile(url: string): Promise<ArrayBuffer | null> {
  try {
    const response = await fetch(url);
    if (!response.ok) {
      console.error(`Failed to load ${url}: ${response.statusText}`);
      return null;
    }
    return await response.arrayBuffer();
  } catch (error) {
    console.error(`Error loading ${url}:`, error);
    return null;
  }
}

export function uploadCompressedTexture(
  gl: WebGL2RenderingContext,
  ktxData: KTXTextureData,
  format: CompressedFormatInfo
): WebGLTexture | null {
  const texture = gl.createTexture();
  if (!texture) {
    console.error('Failed to create WebGL texture');
    return null;
  }

  gl.bindTexture(gl.TEXTURE_2D, texture);

  // Check if the format in the KTX file is supported by the current extension
  const internalFormat = ktxData.header.glInternalFormat;
  if (!formatSupportsInternalFormat(format, internalFormat)) {
    console.error(`KTX internal format ${internalFormat} not supported by ${format.name}`);
    gl.deleteTexture(texture);
    return null;
  }

  // Upload all mipmap levels
  for (let i = 0; i < ktxData.mipmaps.length; i++) {
    const mip = ktxData.mipmaps[i];
    gl.compressedTexImage2D(
      gl.TEXTURE_2D,
      i,
      internalFormat,
      mip.width,
      mip.height,
      0,
      mip.data
    );
  }

  // Set texture parameters
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER,
    ktxData.mipmaps.length > 1 ? gl.LINEAR_MIPMAP_LINEAR : gl.LINEAR);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);

  gl.bindTexture(gl.TEXTURE_2D, null);

  return texture;
}

export async function loadAndUploadTexture(
  gl: WebGL2RenderingContext,
  baseUrl: string,
  formats: CompressedFormatInfo[]
): Promise<{ texture: WebGLTexture; ktxData: KTXTextureData; formatUsed: CompressedFormatInfo } | null> {
  // Try each format in order until we find one that works
  for (const format of formats) {
    if (!format.extension) continue;

    const url = `${baseUrl}_${format.fileExtension}.ktx`;
    console.log(`Attempting to load: ${url}`);

    const arrayBuffer = await loadKTXFile(url);
    if (!arrayBuffer) {
      console.log(`File not found or failed to load: ${url}`);
      continue;
    }

    const ktxData = parseKTX(arrayBuffer);
    if (!ktxData) {
      console.error(`Failed to parse KTX file: ${url}`);
      continue;
    }

    // Check if this format can handle the internal format in the file
    if (!formatSupportsInternalFormat(format, ktxData.header.glInternalFormat)) {
      console.log(`Format ${format.name} doesn't support internal format 0x${ktxData.header.glInternalFormat.toString(16)}`);
      continue;
    }

    const texture = uploadCompressedTexture(gl, ktxData, format);
    if (!texture) {
      console.error(`Failed to upload texture: ${url}`);
      continue;
    }

    console.log(`✓ Successfully loaded ${url}`);
    console.log(`  Texture size: ${ktxData.header.pixelWidth}x${ktxData.header.pixelHeight}`);
    console.log(`  Mipmap levels: ${ktxData.mipmaps.length}`);
    console.log(`  Internal format: 0x${ktxData.header.glInternalFormat.toString(16)}`);
    console.log(`  Format used: ${format.name}`);

    return { texture, ktxData, formatUsed: format };
  }

  console.error('No compatible texture file found');
  return null;
}
