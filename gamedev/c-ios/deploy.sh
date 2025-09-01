#!/bin/bash

# iOS Deployment Script
# Usage: ./deploy.sh [device|sim] [bundle-id]

set -e

DEVICE_TYPE=${1:-device}
BUNDLE_ID=${2:-com.example.clearsapp}

echo "🚀 iOS Deployment Script"
echo "Target: $DEVICE_TYPE"
echo "Bundle ID: $BUNDLE_ID"

# Function to list available devices
list_devices() {
    echo "📱 Connected devices:"
    xcrun devicectl list devices | grep -E "(iPhone|iPad)"
}

# Function to list available simulators  
list_simulators() {
    echo "📱 Available simulators:"
    xcrun simctl list devices available | grep -E "iPhone|iPad" | grep -E "Booted|Shutdown"
}

# Function to find provisioning profiles
find_provisioning_profile() {
    local bundle_id="$1"
    
    echo "🔍 Searching for provisioning profile matching: $bundle_id"
    
    for profile in ~/Library/Developer/Xcode/UserData/Provisioning\ Profiles/*.mobileprovision; do
        if [ -f "$profile" ]; then
            # Extract the application identifier from the profile
            app_id=$(security cms -D -i "$profile" 2>/dev/null | plutil -extract Entitlements.application-identifier raw - 2>/dev/null || echo "")
            
            if [[ "$app_id" == *"$bundle_id"* ]]; then
                echo "✅ Found matching profile: $(basename "$profile")"
                echo "$profile"
                return 0
            fi
        fi
    done
    
    echo "❌ No matching provisioning profile found for $bundle_id"
    echo "Available profiles:"
    for profile in ~/Library/Developer/Xcode/UserData/Provisioning\ Profiles/*.mobileprovision; do
        if [ -f "$profile" ]; then
            app_id=$(security cms -D -i "$profile" 2>/dev/null | plutil -extract Entitlements.application-identifier raw - 2>/dev/null || echo "unknown")
            echo "  - $app_id ($(basename "$profile"))"
        fi
    done
    return 1
}

# Function to get signing identity
get_signing_identity() {
    echo "🔑 Available signing identities:"
    security find-identity -v -p codesigning | grep "Apple Development"
    
    # Get the first Apple Development identity
    identity=$(security find-identity -v -p codesigning | grep "Apple Development" | head -1 | sed -E 's/.*"(.*)"/\1/')
    
    if [ -z "$identity" ]; then
        echo "❌ No Apple Development signing identity found"
        exit 1
    fi
    
    echo "✅ Using identity: $identity"
    echo "$identity"
}

case $DEVICE_TYPE in
    "device"|"dev")
        echo "📲 Building for iOS device..."
        
        # Find provisioning profile
        if ! PROFILE=$(find_provisioning_profile "$BUNDLE_ID"); then
            echo "💡 Tip: Create a provisioning profile with bundle ID '$BUNDLE_ID' in Xcode or Apple Developer Portal"
            exit 1
        fi
        
        # Get signing identity
        IDENTITY=$(get_signing_identity)
        
        # Update Makefile temporarily or use environment variables
        export IOS_PROFILE="$PROFILE"
        export IOS_IDENTITY="$IDENTITY"
        export IOS_BUNDLE_ID="$BUNDLE_ID"
        
        # Build
        make ios
        
        # Auto-select first connected iOS device (prioritize connected over available, skip unavailable)
        DEVICE_ID=$(xcrun devicectl list devices | grep -E "(iPhone|iPad)" | grep -v "unavailable" | grep -E "(available|connected)" | head -1 | grep -o '[A-F0-9-]\{36\}')
        
        if [ -z "$DEVICE_ID" ]; then
            list_devices
            echo "❌ No connected iOS devices found"
            echo "💡 Make sure your device is:"
            echo "   - Connected via USB"
            echo "   - Unlocked and trusted this computer"
            echo "   - In Developer Mode (iOS 16+)"
            exit 1
        fi
        
        DEVICE_NAME=$(xcrun devicectl list devices | grep "$DEVICE_ID" | awk '{print $1}')
        echo "📲 Using device: $DEVICE_NAME ($DEVICE_ID)"
        
        echo "📲 Installing on device: $DEVICE_ID"
        xcrun devicectl device install app --device "$DEVICE_ID" build/ClearSapp.app
        
        echo "📱 App installed successfully!"
        # echo "🚀 Launching app in 2 seconds..."
        # sleep 2
        
        # Use the actual bundle ID from Info.plist
        # ACTUAL_BUNDLE_ID=$(plutil -extract CFBundleIdentifier raw Info.plist)
        # xcrun devicectl device process launch --device "$DEVICE_ID" --start-stopped "$ACTUAL_BUNDLE_ID"
        
        ;;
        
    "sim"|"simulator")
        echo "📲 Building for iOS simulator..."
        
        # Build for simulator  
        make ios-sim
        
        # List simulators and let user choose
        list_simulators
        echo ""
        read -p "Enter simulator identifier (or press Enter for first booted): " SIM_ID
        
        if [ -z "$SIM_ID" ]; then
            SIM_ID=$(xcrun simctl list devices | grep iPhone | grep Booted | head -1 | grep -o '[A-F0-9-]\{36\}')
            if [ -z "$SIM_ID" ]; then
                echo "🔄 Booting first available iPhone simulator..."
                SIM_ID=$(xcrun simctl list devices available | grep iPhone | head -1 | grep -o '[A-F0-9-]\{36\}')
                xcrun simctl boot "$SIM_ID"
            fi
        fi
        
        echo "📲 Installing on simulator: $SIM_ID"
        xcrun simctl install "$SIM_ID" build/ClearSapp.app
        
        # echo "🚀 Launching app..."
        # # Use the actual bundle ID from Info.plist
        # ACTUAL_BUNDLE_ID=$(plutil -extract CFBundleIdentifier raw Info.plist)
        # xcrun simctl launch "$SIM_ID" "$ACTUAL_BUNDLE_ID"
        
        ;;
        
    *)
        echo "❌ Usage: $0 [device|sim] [bundle-id]"
        echo ""
        echo "Examples:"
        echo "  $0 device com.yourcompany.yourapp"
        echo "  $0 sim"
        echo "  $0 device"
        exit 1
        ;;
esac

echo "✅ Deployment complete!"