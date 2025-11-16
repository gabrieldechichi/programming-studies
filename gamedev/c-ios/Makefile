CC = clang

BUILD_CFLAGS = -std=c99 \
			   -O2 \
               -Wall -Wextra -Werror \
               -Wpedantic -Wcast-align -Wcast-qual \
               -Wconversion -Wenum-compare -Wfloat-equal \
               -Wredundant-decls -Wsign-conversion \
               -Wstrict-prototypes -Wmissing-prototypes \
               -Wold-style-definition -Wmissing-declarations \
               -Wformat=2 -Wformat-security \
               -Wundef -Wshadow \
               -I./vendor

# Build the build system
build: build.c
	$(CC) $(BUILD_CFLAGS) build.c -o build

macos: build
	./build macos

macos-release: build
	./build macos release

ios: build
	./build ios

ios-release: build
	./build ios release

ios-deploy: build
	./build ios-deploy

ios-deploy-release: build
	./build ios-deploy release

windows: build
	./build windows

windows-release: build
	./build windows release

shaders: build
	./build shaders

compile-commands: build
	bear -- ./build macos

clean:
	rm -rf out/
	rm -f build
	rm -f src/shaders/*.h

all: macos ios windows
all-release: macos-release ios-release windows-release

.PHONY: app macos ios windows ios-deploy shaders clean all compile-commands