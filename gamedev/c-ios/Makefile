CC = clang
MACOS_CFLAGS = -x objective-c -framework Cocoa -framework QuartzCore -framework Metal -framework MetalKit
IOS_SIM_CFLAGS = -x objective-c -framework Foundation -framework UIKit -framework QuartzCore -framework Metal -framework MetalKit -miphonesimulator-version-min=12.0
IOS_CFLAGS = -x objective-c -framework Foundation -framework UIKit -framework QuartzCore -framework Metal -framework MetalKit -miphoneos-version-min=12.0
TARGET = clear-sapp
SOURCE = clear-sapp.c
APP_BUNDLE = ClearSapp.app

# Configurable variables (can be overridden)
BUNDLE_ID ?= com.example.clearsapp.Test
SIGNING_IDENTITY ?= Apple Development: gabriel.dechichi@portola.ai (8Y3X5XDMMD)
PROVISIONING_PROFILE ?= ~/Library/Developer/Xcode/UserData/Provisioning\ Profiles/4d20f01c-5581-46d3-a2ad-7a07adcf0c84.mobileprovision

# macOS build
$(TARGET): $(SOURCE)
	$(CC) $(MACOS_CFLAGS) $(SOURCE) -o $(TARGET)

# iOS simulator build
ios-sim: $(SOURCE)
	xcrun -sdk iphonesimulator $(CC) $(IOS_SIM_CFLAGS) -arch arm64 $(SOURCE) -o $(TARGET)-sim
	rm -rf $(APP_BUNDLE)
	mkdir -p $(APP_BUNDLE)
	cp $(TARGET)-sim $(APP_BUNDLE)/$(TARGET)
	cp Info.plist $(APP_BUNDLE)/Info.plist

# iOS device build
ios: $(SOURCE)
	xcrun -sdk iphoneos $(CC) $(IOS_CFLAGS) -arch arm64 $(SOURCE) -o $(TARGET)-ios
	rm -rf $(APP_BUNDLE)
	mkdir -p $(APP_BUNDLE)
	cp $(TARGET)-ios $(APP_BUNDLE)/$(TARGET)
	cp Info.plist $(APP_BUNDLE)/Info.plist
	cp $(PROVISIONING_PROFILE) $(APP_BUNDLE)/embedded.mobileprovision
	codesign -s "$(SIGNING_IDENTITY)" --timestamp -f --entitlements Entitlements.plist $(APP_BUNDLE)

clean:
	rm -f $(TARGET) $(TARGET)-sim $(TARGET)-ios
	rm -rf $(APP_BUNDLE)

run: $(TARGET)
	./$(TARGET)

.PHONY: clean run ios ios-sim