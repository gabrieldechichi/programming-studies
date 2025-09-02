CC = clang

# Build the build system
build: build.c
	$(CC) -O2 build.c -o build

# Use the build system to build targets
app: build
	./build

macos: build
	./build macos

ios: build
	./build ios

ios-deploy: build
	./build ios-deploy

clean:
	rm -rf out/
	rm -f build

.PHONY: app macos ios ios-deploy clean