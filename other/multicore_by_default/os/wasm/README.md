# WASM Browser Test

Minimal browser runtime for the WASM build, following the handmade-games pattern.

## Build WASM

```bash
./hz_build wasm          # Debug build
./hz_build wasm release  # Release build
```

## Run in Browser

```bash
# Start a local server (from project root)
python3 -m http.server 8000

# Open browser to:
# http://localhost:8000/src/os/wasm/
```

## How It Works

The JavaScript runtime provides:

### Memory Management
- Shared `WebAssembly.Memory` (1GB)
- Memory imported by WASM (`--import-memory` flag)
- Data structures synchronized via typed arrays

### Platform Data (Written to WASM Memory Each Frame)
- **GameTime**: `now` (f32), `dt` (f32)
- **GameCanvas**: `width` (u32), `height` (u32)

### Game Loop (60 FPS)
```javascript
1. Update time/canvas in memory
2. Call game_update_and_render(GameMemoryOffset)
3. requestAnimationFrame(tick)
```

### Platform Functions
- `_platform_log_info/warn/error` - Console logging from C code

## Memory Layout

```
GameMemoryOffset = platform_get_heap_base()
├─ GameTime      (offset: 0, size: 8 bytes)
│  ├─ now  (f32)
│  └─ dt   (f32)
└─ GameCanvas    (offset: 8, size: 8 bytes)
   ├─ width  (u32)
   └─ height (u32)
```

## Console Access

All WASM exports are available at `window.wasmExports`:

```javascript
// In browser console:
wasmExports.game_init(0);
wasmExports.platform_get_heap_base();
```

## Files

- `index.html` - HTML page with canvas and output console
- `main.js` - WASM loader, memory sync, and game loop
