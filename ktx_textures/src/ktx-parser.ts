// KTX file format constants
const KTX_IDENTIFIER = [0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A];
const HEADER_LENGTH = 64;

export interface KTXHeader {
  glType: number;
  glTypeSize: number;
  glFormat: number;
  glInternalFormat: number;
  glBaseInternalFormat: number;
  pixelWidth: number;
  pixelHeight: number;
  pixelDepth: number;
  numberOfArrayElements: number;
  numberOfFaces: number;
  numberOfMipmapLevels: number;
  bytesOfKeyValueData: number;
}

export interface KTXMipmapLevel {
  data: Uint8Array;
  width: number;
  height: number;
}

export interface KTXTextureData {
  header: KTXHeader;
  mipmaps: KTXMipmapLevel[];
}

function readUint32(view: DataView, offset: number, littleEndian: boolean): number {
  return view.getUint32(offset, littleEndian);
}

export function parseKTX(arrayBuffer: ArrayBuffer): KTXTextureData | null {
  const view = new DataView(arrayBuffer);

  // Verify KTX identifier
  for (let i = 0; i < 12; i++) {
    if (view.getUint8(i) !== KTX_IDENTIFIER[i]) {
      console.error('Invalid KTX file identifier');
      return null;
    }
  }

  // Check endianness
  const endianness = view.getUint32(12, true);
  const littleEndian = endianness === 0x04030201;

  // Parse header
  const header: KTXHeader = {
    glType: readUint32(view, 16, littleEndian),
    glTypeSize: readUint32(view, 20, littleEndian),
    glFormat: readUint32(view, 24, littleEndian),
    glInternalFormat: readUint32(view, 28, littleEndian),
    glBaseInternalFormat: readUint32(view, 32, littleEndian),
    pixelWidth: readUint32(view, 36, littleEndian),
    pixelHeight: readUint32(view, 40, littleEndian),
    pixelDepth: readUint32(view, 44, littleEndian),
    numberOfArrayElements: readUint32(view, 48, littleEndian),
    numberOfFaces: readUint32(view, 52, littleEndian),
    numberOfMipmapLevels: readUint32(view, 56, littleEndian),
    bytesOfKeyValueData: readUint32(view, 60, littleEndian),
  };

  // Ensure we have at least 1 mipmap level
  if (header.numberOfMipmapLevels === 0) {
    header.numberOfMipmapLevels = 1;
  }

  // Skip key/value data
  let offset = HEADER_LENGTH + header.bytesOfKeyValueData;

  // Parse mipmap levels
  const mipmaps: KTXMipmapLevel[] = [];
  let width = header.pixelWidth;
  let height = header.pixelHeight;

  for (let i = 0; i < header.numberOfMipmapLevels; i++) {
    const imageSize = readUint32(view, offset, littleEndian);
    offset += 4;

    const data = new Uint8Array(arrayBuffer, offset, imageSize);
    mipmaps.push({ data, width, height });

    offset += imageSize;

    // Mipmap padding to 4-byte alignment
    const padding = (imageSize + 3) & ~3;
    offset += padding - imageSize;

    // Next mipmap dimensions
    width = Math.max(1, width >> 1);
    height = Math.max(1, height >> 1);
  }

  return { header, mipmaps };
}
