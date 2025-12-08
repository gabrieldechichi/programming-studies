EMSDK = D:/dev/tools/emsdk
CC = $(EMSDK)/upstream/emscripten/emcc
CFLAGS = -O0
LDFLAGS = -pthread -s PROXY_TO_PTHREAD -s EXIT_RUNTIME=1 -s WASM=1 -s MODULARIZE=1 -s EXPORT_ES6=1 -s EXPORT_NAME='createModule'

SRC = main.c
OUT_DIR = out
WASM_OUT = $(OUT_DIR)/wasm.mjs
TS_OUT = $(OUT_DIR)/main.mjs

all: $(OUT_DIR) $(WASM_OUT) $(TS_OUT)

$(OUT_DIR):
	if not exist $(OUT_DIR) mkdir $(OUT_DIR)

$(WASM_OUT): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(WASM_OUT) $(LDFLAGS)

$(TS_OUT): main.ts $(WASM_OUT)
	bun build main.ts --outfile $(TS_OUT) --external ./wasm.mjs

clean:
	if exist $(OUT_DIR) rmdir /s /q $(OUT_DIR)

.PHONY: all clean
