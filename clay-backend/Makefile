CC = clang

CFLAGS = -Wall -Wextra -std=c99 -DCOMPILER_CLANG -DWASM
CFLAGS_DEBUG = -O0 -g -DDEBUG
CFLAGS_RELEASE = -O2
INCLUDE_FLAGS = -Isrc -Ivendor -Ios -Imeta -I./ -Igenerated
LINK_FLAGS =

SRC = src/main.c
WASM_TARGET = web/dist/app.wasm
DIST_DIR = web/dist

all: build

build-wasm:
	@mkdir -p $(DIST_DIR)
	time $(CC) $(INCLUDE_FLAGS) $(CFLAGS) $(CFLAGS_DEBUG) --target=wasm32 \
		-nostdlib \
		-Wl,--no-entry \
		-Wl,--export-all \
		-Wl,--allow-undefined \
		-Wl,--import-memory \
		-o $(WASM_TARGET) \
		$(SRC)

build-ts:
	@mkdir -p $(DIST_DIR)
	cd web && bun build main.ts --outdir dist

build-wasm-release:
	@mkdir -p $(DIST_DIR)
	time $(CC) $(INCLUDE_FLAGS) $(CFLAGS) $(CFLAGS_RELEASE) --target=wasm32 \
		-nostdlib \
		-Wl,--no-entry \
		-Wl,--export-all \
		-Wl,--allow-undefined \
		-Wl,--import-memory \
		-o $(WASM_TARGET) \
		$(SRC)

build-ts-release:
	@mkdir -p $(DIST_DIR)
	cd web && bun build main.ts --outdir dist --minify --target browser

copy-static:
	@mkdir -p $(DIST_DIR)
	cp web/index.html $(DIST_DIR)/
	cp web/*.glsl.js $(DIST_DIR)/
	cp web/*.hza $(DIST_DIR)/ 2>/dev/null || true
	cp web/*.png $(DIST_DIR)/ 2>/dev/null || true
	cp web/*.json $(DIST_DIR)/ 2>/dev/null || true
	cp web/*.ttf $(DIST_DIR)/ 2>/dev/null || true

build: build-wasm build-ts copy-static

build-release: build-wasm-release build-ts-release copy-static

clean:
	rm -rf $(DIST_DIR)

serve:
	@echo "Starting web server at http://localhost:8000"
	@cd $(DIST_DIR) && python3 -m http.server 8000

# Tool: hza_gen - Generate .hza binary font assets
tools/hza_gen: tools/hza_gen.c
	@mkdir -p tools
	$(CC) -o tools/hza_gen tools/hza_gen.c -Isrc -Ivendor -Wall -Wextra -std=c99 -UDWASM

hza-gen: tools/hza_gen

.PHONY: all clean build-wasm build-ts build-wasm-release build-ts-release copy-static build build-release serve hza-gen
