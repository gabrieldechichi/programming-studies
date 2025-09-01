CC = clang

# Build the build system
build: build.c
	$(CC) -O2 build.c -o build

# Use the build system to build the app
app: build
	./build

clean:
	rm -rf out/
	rm -f build

.PHONY: app clean