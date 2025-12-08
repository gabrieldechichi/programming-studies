CC = clang
CFLAGS = --target=wasm32 -nostdlib -O0 -I. -DWASM
LDFLAGS = -Wl,--no-entry -Wl,--export=main -Wl,--import-memory -Wl,--allow-undefined

SRC = main.c
OUT_DIR = out
WASM_OUT = $(OUT_DIR)/wasm.wasm
JS_OUT = $(OUT_DIR)/main.mjs

all: $(OUT_DIR) $(WASM_OUT) $(JS_OUT)

$(OUT_DIR):
	powershell -Command "if (!(Test-Path $(OUT_DIR))) { New-Item -ItemType Directory -Path $(OUT_DIR) }"

$(WASM_OUT): $(SRC) os/os_wasm.c
	$(CC) $(CFLAGS) $(SRC) -o $(WASM_OUT) $(LDFLAGS)

$(JS_OUT): main.ts
	bun build main.ts --outfile $(JS_OUT)

run: all
	bun run main.ts

clean:
	powershell -Command "if (Test-Path $(OUT_DIR)) { Remove-Item -Recurse -Force $(OUT_DIR) }"

.PHONY: all clean run
