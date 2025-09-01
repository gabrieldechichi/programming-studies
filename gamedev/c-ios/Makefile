# Detect platform
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    PLATFORM = darwin
    DEFAULT_TARGET = macos
else ifeq ($(UNAME_S),Linux)
    PLATFORM = linux
    DEFAULT_TARGET = linux
else
    PLATFORM = windows
    DEFAULT_TARGET = windows
endif

CC = clang
MACOS_CFLAGS = -x objective-c -Isrc -Isrc/vendor
MACOS_LDFLAGS = -framework Cocoa -framework QuartzCore -framework Metal -framework MetalKit
MACOS_LINK_FLAGS = -x objective-c
IOS_CFLAGS = -x objective-c -miphoneos-version-min=12.0 -Isrc -Isrc/vendor
IOS_LDFLAGS = -framework Foundation -framework UIKit -framework QuartzCore -framework Metal -framework MetalKit
IOS_LINK_FLAGS = -x objective-c

# Directories
SRC_DIR = src
BUILD_DIR = build
MACOS_BUILD_DIR = $(BUILD_DIR)/macos
IOS_BUILD_DIR = $(BUILD_DIR)/ios

# Targets and sources
TARGET = app
MAIN_SOURCE = $(SRC_DIR)/main.c
VENDOR_SOURCE = $(SRC_DIR)/vendor/vendor.c
MACOS_VENDOR_OBJ = $(MACOS_BUILD_DIR)/vendor.o
IOS_VENDOR_OBJ = $(IOS_BUILD_DIR)/vendor.o
APP_BUNDLE = $(IOS_BUILD_DIR)/ClearSapp.app

# Configurable variables (can be overridden)
BUNDLE_ID ?= com.example.clearsapp.Test
SIGNING_IDENTITY ?= Apple Development: gabriel.dechichi@portola.ai (8Y3X5XDMMD)
PROVISIONING_PROFILE ?= ~/Library/Developer/Xcode/UserData/Provisioning\ Profiles/4d20f01c-5581-46d3-a2ad-7a07adcf0c84.mobileprovision

# Default target based on platform
.DEFAULT_GOAL := $(DEFAULT_TARGET)
# Platform info
platform-info:
	@echo "Detected platform: $(PLATFORM)"
	@echo "Default target: $(DEFAULT_TARGET)"

# Ensure build directories exist
$(MACOS_BUILD_DIR):
	mkdir -p $(MACOS_BUILD_DIR)

$(IOS_BUILD_DIR):
	mkdir -p $(IOS_BUILD_DIR)

# Compile vendor object files
$(MACOS_VENDOR_OBJ): $(VENDOR_SOURCE) | $(MACOS_BUILD_DIR)
	$(CC) $(MACOS_CFLAGS) -c $(VENDOR_SOURCE) -o $(MACOS_VENDOR_OBJ)

$(IOS_VENDOR_OBJ): $(VENDOR_SOURCE) | $(IOS_BUILD_DIR)
	xcrun -sdk iphoneos $(CC) $(IOS_CFLAGS) -arch arm64 -c $(VENDOR_SOURCE) -o $(IOS_VENDOR_OBJ)

# macOS build
macos: $(MACOS_VENDOR_OBJ) | $(MACOS_BUILD_DIR)
	$(CC) $(MACOS_LINK_FLAGS) -Isrc -Isrc/vendor $(MAIN_SOURCE) -x none $(MACOS_VENDOR_OBJ) -o $(MACOS_BUILD_DIR)/$(TARGET) $(MACOS_LDFLAGS)

# iOS device build
ios: $(IOS_VENDOR_OBJ) | $(IOS_BUILD_DIR)
	xcrun -sdk iphoneos $(CC) $(IOS_LINK_FLAGS) -arch arm64 -Isrc -Isrc/vendor $(MAIN_SOURCE) -x none $(IOS_VENDOR_OBJ) -o $(IOS_BUILD_DIR)/$(TARGET)-ios $(IOS_LDFLAGS)
	rm -rf $(APP_BUNDLE)
	mkdir -p $(APP_BUNDLE)
	cp $(IOS_BUILD_DIR)/$(TARGET)-ios $(APP_BUNDLE)/$(TARGET)
	cp Info.plist $(APP_BUNDLE)/Info.plist
	cp $(PROVISIONING_PROFILE) $(APP_BUNDLE)/embedded.mobileprovision
	codesign -s "$(SIGNING_IDENTITY)" --timestamp -f --entitlements Entitlements.plist $(APP_BUNDLE)

clean:
	rm -rf $(BUILD_DIR)

all: macos ios

run: $(DEFAULT_TARGET)
	./$(MACOS_BUILD_DIR)/$(TARGET)

.PHONY: clean run ios