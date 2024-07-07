TARGET=build/pacman
TARGET_WASM=$(TARGET)-wasm.wasm
DEBUGFLAGS=-debug -o:none

build-debug-wasm:
	mkdir -p $(TARGET)
	odin build . -target:js_wasm32 $(DEBUGFLAGS) -out:$(TARGET_WASM)

hot-debug-wasm: build-debug-wasm
	live-server --watch=$(TARGET_WASM)
