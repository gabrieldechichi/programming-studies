CC = clang
MACOS_CFLAGS = -x objective-c -framework Cocoa -framework QuartzCore -framework Metal -framework MetalKit -Isrc
IOS_SIM_CFLAGS = -x objective-c -framework Foundation -framework UIKit -framework QuartzCore -framework Metal -framework MetalKit -miphonesimulator-version-min=12.0 -Isrc
IOS_CFLAGS = -x objective-c -framework Foundation -framework UIKit -framework QuartzCore -framework Metal -framework MetalKit -miphoneos-version-min=12.0 -Isrc

# Directories
SRC_DIR = src
BUILD_DIR = build

# Targets and sources
TARGET = clear-sapp
SOURCE = $(SRC_DIR)/triangle-sapp.c
TRIANGLE_TARGET = triangle-sapp
TRIANGLE_SOURCE = $(SRC_DIR)/triangle-sapp.c
APP_BUNDLE = $(BUILD_DIR)/ClearSapp.app

# Configurable variables (can be overridden)
BUNDLE_ID ?= com.example.clearsapp.Test
SIGNING_IDENTITY ?= Apple Development: gabriel.dechichi@portola.ai (8Y3X5XDMMD)
PROVISIONING_PROFILE ?= ~/Library/Developer/Xcode/UserData/Provisioning\ Profiles/4d20f01c-5581-46d3-a2ad-7a07adcf0c84.mobileprovision

# Ensure build directory exists
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# macOS build
$(TARGET): $(SOURCE) | $(BUILD_DIR)
	$(CC) $(MACOS_CFLAGS) $(SOURCE) -o $(BUILD_DIR)/$(TARGET)

$(TRIANGLE_TARGET): $(TRIANGLE_SOURCE) | $(BUILD_DIR)
	$(CC) $(MACOS_CFLAGS) $(TRIANGLE_SOURCE) -o $(BUILD_DIR)/$(TRIANGLE_TARGET)

# iOS simulator build
ios-sim: $(SOURCE) | $(BUILD_DIR)
	xcrun -sdk iphonesimulator $(CC) $(IOS_SIM_CFLAGS) -arch arm64 $(SOURCE) -o $(BUILD_DIR)/$(TARGET)-sim
	rm -rf $(APP_BUNDLE)
	mkdir -p $(APP_BUNDLE)
	cp $(BUILD_DIR)/$(TARGET)-sim $(APP_BUNDLE)/$(TARGET)
	cp Info.plist $(APP_BUNDLE)/Info.plist

# iOS device build with triangle app
triangle-ios: $(TRIANGLE_SOURCE) | $(BUILD_DIR)
	xcrun -sdk iphoneos $(CC) $(IOS_CFLAGS) -arch arm64 $(TRIANGLE_SOURCE) -o $(BUILD_DIR)/$(TRIANGLE_TARGET)-ios
	rm -rf $(APP_BUNDLE)
	mkdir -p $(APP_BUNDLE)
	cp $(BUILD_DIR)/$(TRIANGLE_TARGET)-ios $(APP_BUNDLE)/clear-sapp
	cp Info.plist $(APP_BUNDLE)/Info.plist
	cp $(PROVISIONING_PROFILE) $(APP_BUNDLE)/embedded.mobileprovision
	codesign -s "$(SIGNING_IDENTITY)" --timestamp -f --entitlements Entitlements.plist $(APP_BUNDLE)

# iOS device build
ios: $(SOURCE) | $(BUILD_DIR)
	xcrun -sdk iphoneos $(CC) $(IOS_CFLAGS) -arch arm64 $(SOURCE) -o $(BUILD_DIR)/$(TARGET)-ios
	rm -rf $(APP_BUNDLE)
	mkdir -p $(APP_BUNDLE)
	cp $(BUILD_DIR)/$(TARGET)-ios $(APP_BUNDLE)/$(TARGET)
	cp Info.plist $(APP_BUNDLE)/Info.plist
	cp $(PROVISIONING_PROFILE) $(APP_BUNDLE)/embedded.mobileprovision
	codesign -s "$(SIGNING_IDENTITY)" --timestamp -f --entitlements Entitlements.plist $(APP_BUNDLE)

clean:
	rm -rf $(BUILD_DIR)

run: $(TARGET)
	./$(BUILD_DIR)/$(TARGET)

.PHONY: clean run ios ios-sim