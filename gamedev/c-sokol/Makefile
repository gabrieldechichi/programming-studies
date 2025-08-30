CC = clang
TARGET = main

# Platform detection
PLATFORM := $(shell uname -s)

MAIN_SOURCE = main.c
VENDOR_SOURCE = vendor/vendor.c

# Platform-specific settings
ifeq ($(PLATFORM),Darwin)
    # macOS: Use Metal backend, compile .c files as Objective-C
    BUILD_DIR = build/macos
    LDFLAGS = -framework Cocoa -framework QuartzCore -framework Metal -framework MetalKit -framework AudioToolbox
    MAIN_CFLAGS = -fobjc-arc -x objective-c
    VENDOR_CFLAGS = -fobjc-arc -x objective-c
    LINK_CFLAGS = -fobjc-arc
    LINK_LANG_FLAGS = -x objective-c
    LINK_RESET_FLAGS = -x none
else ifeq ($(PLATFORM),Linux)
    # Linux: Use OpenGL backend and pure C
    BUILD_DIR = build/linux
    LDFLAGS = -lX11 -lXi -lXcursor -lGL -ldl -lpthread -lm
    MAIN_CFLAGS = 
    VENDOR_CFLAGS =
    LINK_CFLAGS =
    LINK_LANG_FLAGS = 
    LINK_RESET_FLAGS =
else
    # Windows: Use D3D11 backend and pure C
    BUILD_DIR = build/windows
    LDFLAGS = -lgdi32 -lole32 -ld3d11 -ldxgi
    MAIN_CFLAGS =
    VENDOR_CFLAGS =
    LINK_CFLAGS =
    LINK_LANG_FLAGS =
    LINK_RESET_FLAGS =
endif

# Build paths
TARGET_PATH = $(BUILD_DIR)/$(TARGET)
VENDOR_OBJ = $(BUILD_DIR)/vendor.o

# Strict warning flags for main code (common across platforms)
MAIN_CFLAGS += -std=c99 \
               -Wall -Wextra -Werror \
               -Wpedantic -Wcast-align -Wcast-qual \
               -Wconversion -Wenum-compare -Wfloat-equal \
               -Wredundant-decls -Wsign-conversion \
               -Wstrict-prototypes -Wmissing-prototypes \
               -Wold-style-definition -Wmissing-declarations \
               -Wformat=2 -Wformat-security \
               -Wundef -Wshadow \
               -I./vendor

# Relaxed warning flags for vendor code (common across platforms)
VENDOR_CFLAGS += -std=c99 \
                 -Wall -Wextra \
                 -Wno-implicit-float-conversion \
                 -Wno-implicit-int-float-conversion \
                 -Wno-enum-enum-conversion \
                 -I./vendor

# Debug build by default
MAIN_CFLAGS += -g -O0 -DDEBUG
VENDOR_CFLAGS += -g -O0 -DDEBUG

.PHONY: all clean run

all: $(TARGET_PATH)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(VENDOR_OBJ): $(VENDOR_SOURCE) | $(BUILD_DIR)
	$(CC) $(VENDOR_CFLAGS) -c $(VENDOR_SOURCE) -o $(VENDOR_OBJ)

$(TARGET_PATH): $(MAIN_SOURCE) $(VENDOR_OBJ) | $(BUILD_DIR)
	$(CC) $(LINK_CFLAGS) -I./vendor $(LINK_LANG_FLAGS) $(MAIN_SOURCE) $(LINK_RESET_FLAGS) $(VENDOR_OBJ) -o $(TARGET_PATH) $(LDFLAGS)

clean:
	rm -rf build/

run: $(TARGET_PATH)
	$(TARGET_PATH)