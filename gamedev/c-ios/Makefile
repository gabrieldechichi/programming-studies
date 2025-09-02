CC = clang

# Build the build system
build: build.c
	$(CC) -O2 build.c -o build

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

compile-commands: build
	bear -- ./build macos

clean:
	rm -rf out/
	rm -f build

all: macos ios windows
all-release: macos-release ios-release windows-release

.PHONY: app macos ios windows ios-deploy clean all compile-commands