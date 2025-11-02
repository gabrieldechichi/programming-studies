CC = clang

CFLAGS = -Wall -Wextra -std=c99 -DCOMPILER_CLANG -DWASM
CFLAGS_DEBUG = -O0 -g -DDEBUG
CFLAGS_RELEASE = -O2
INCLUDE_FLAGS = -Isrc -Ivendor -Ios -Imeta -I./ -Igenerated
LINK_FLAGS =

SRC = src/main.c
WASM_TARGET = web/app.wasm

all: build-wasm

build:
	$(CC) $(INCLUDE_FLAGS) $(CFLAGS) $(CFLAGS_DEBUG) --target=wasm32 \
		-nostdlib \
		-Wl,--no-entry \
		-Wl,--export-all \
		-Wl,--allow-undefined \
		-Wl,--import-memory \
		-o $(WASM_TARGET) \
		$(SRC)

clean:
	rm -f $(WASM_TARGET)

serve:
	@echo "Starting web server at http://localhost:8000"
	@cd web && python3 -m http.server 8000

.PHONY: all clean build-wasm serve
