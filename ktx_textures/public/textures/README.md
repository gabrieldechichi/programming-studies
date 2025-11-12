# KTX Texture Files

Place your KTX compressed texture files in this directory.

## File Naming Convention

The application expects texture files to follow this naming pattern:
```
<basename>_<format>.ktx
```

For example, if the base URL in main.ts is `/textures/test`, the application will look for:
- `test_dxt5.ktx` - S3TC/DXT5 compressed (desktop)
- `test_etc2.ktx` - ETC2 compressed (mobile)
- `test_astc.ktx` - ASTC compressed (modern mobile)
- `test_pvrtc.ktx` - PVRTC compressed (iOS)
- `test_etc1.ktx` - ETC1 compressed (older mobile)

## Supported Formats

The application automatically detects which compressed texture formats your GPU supports and loads the appropriate file.

### Desktop (Windows/Mac/Linux)
- **S3TC (DXT)**: Most common on desktop GPUs
  - File extension: `_dxt5.ktx`
  - Format: RGBA DXT5

### Mobile (Android/iOS)
- **ETC2**: Baseline format for OpenGL ES 3.0+
  - File extension: `_etc2.ktx`
  - Format: RGBA ETC2 EAC

- **ASTC**: Modern mobile GPUs
  - File extension: `_astc.ktx`
  - Format: RGBA ASTC 4x4

- **PVRTC**: Apple iOS devices
  - File extension: `_pvrtc.ktx`
  - Format: RGBA PVRTC 4BPP

## Creating KTX Files

You can create KTX files using tools like:

1. **PVRTexTool** (PowerVR)
   - Download from: https://www.imaginationtech.com/developers/powervr-sdk-tools/pvrtextool/
   - Supports all major compression formats
   - GUI and command-line interface

2. **Mali Texture Compression Tool**
   - Download from ARM website
   - Good for ETC/ASTC compression

3. **Compressonator** (AMD)
   - Download from: https://gpuopen.com/compressonator/
   - Free and open-source

4. **toktx** (Khronos)
   - Command-line tool from Khronos KTX-Software
   - Install: https://github.com/KhronosGroup/KTX-Software

### Example using toktx:
```bash
# Create DXT5 compressed texture
toktx --bcmp --target_type RGBA --t2 test_dxt5.ktx input.png

# Create ETC2 compressed texture
toktx --encode etc --target_type RGBA test_etc2.ktx input.png

# Create ASTC compressed texture
toktx --encode astc --astc_blk_d 4x4 test_astc.ktx input.png
```

## File Requirements

- Must be valid KTX 1.0 format files
- Can include mipmaps (recommended for better quality)
- Should match the internal format expected by the extension
- Recommended to provide multiple format variants for cross-platform support
