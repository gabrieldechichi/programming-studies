# iOS Sokol Development Without Xcode

A complete C-only iOS development setup using sokol.h that bypasses Xcode's build system.

## Quick Start

### For iOS Simulator
```bash
./deploy.sh sim
```

### For iOS Device  
```bash
./deploy.sh device com.yourcompany.yourapp
```

## Manual Build Commands

```bash
# macOS
make

# iOS Simulator
make ios-sim

# iOS Device (requires provisioning)
make ios
```

## Setup for Other Developers

### 1. Update Bundle Identifier
Edit `Info.plist` and change:
```xml
<key>CFBundleIdentifier</key>
<string>com.yourcompany.yourapp</string>
```

### 2. Create Provisioning Profile
- Create iOS project in Xcode with your bundle ID
- Connect your device and build once
- Provisioning profile will be auto-generated

### 3. Override Makefile Variables
```bash
# Override signing identity and profile
make ios BUNDLE_ID="com.yourcompany.app" \
         SIGNING_IDENTITY="Apple Development: Your Name (TEAMID)" \
         PROVISIONING_PROFILE="path/to/your.mobileprovision"
```

### 4. Use Deploy Script
The `deploy.sh` script automatically:
- Finds matching provisioning profiles
- Lists available devices/simulators  
- Handles installation and launching

## File Structure

```
├── clear-sapp.c           # Main application code
├── sokol_*.h              # Sokol headers
├── Info.plist             # iOS app metadata
├── Entitlements.plist     # iOS entitlements
├── Makefile               # Build system
├── deploy.sh              # Deployment script
└── ClearSapp.app/         # Generated app bundle
```

## Troubleshooting

**"No provisioning profile found"**
- Create an iOS project in Xcode with your bundle ID
- Build once to generate provisioning profile

**"Code signature verification failed"** 
- Update `SIGNING_IDENTITY` in Makefile
- Run `security find-identity -v -p codesigning` to list identities

**"Device not found"**
- Run `xcrun devicectl list devices` to see available devices
- Ensure device is connected and trusted

## Dependencies

- macOS with Xcode Command Line Tools
- Apple Developer account (for device deployment)
- Connected iOS device (for device deployment)