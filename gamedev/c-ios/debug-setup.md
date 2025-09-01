# Xcode Debug Setup for Sokol C App

## Method 1: Debug Running App
1. Run `./deploy.sh` to install app on device
2. Xcode → Window → Devices and Simulators  
3. Select iPhone → Find app → Gear icon → "Debug"

## Method 2: Create Xcode Project
1. Create new iOS project in Xcode
2. Add these files to project:
   - `triangle-sapp.c` (main source)
   - `sokol_*.h` (all sokol headers)
   - `Info.plist`
3. Set C compilation flags:
   ```
   -x objective-c
   -framework Foundation -framework UIKit 
   -framework QuartzCore -framework Metal -framework MetalKit
   ```
4. Set bundle identifier to match: `com.example.clearsapp.Test`

## Method 3: Use Build Settings
In your Xcode project:
- **Build Settings** → **Other C Flags**: Add `-DDEBUG=1`
- **Preprocessor Macros**: Add `DEBUG=1`
- **Generate Debug Symbols**: Yes
- **Strip Debug Symbols During Copy**: No

## Debugging Features Available:
- ✅ Console logs (NSLog, printf)
- ✅ Breakpoints in C code
- ✅ Variable inspection
- ✅ Call stack
- ✅ Memory debugging
- ✅ Performance profiling