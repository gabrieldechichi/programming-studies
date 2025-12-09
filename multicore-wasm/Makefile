CC = clang
CFLAGS = --target=wasm32 -nostdlib -O0 -I. -DWASM -matomics -mbulk-memory
# --shared-memory requires --max-memory (shared memory can't grow)
# --export-table needed for indirect function calls (function pointers) from JS
# --export TLS symbols for thread-local storage support
LDFLAGS = -Wl,--no-entry -Wl,--export-table -Wl,--import-memory -Wl,--shared-memory -Wl,--max-memory=4294967296 -Wl,--allow-undefined -Wl,--export=__wasm_init_tls -Wl,--export=__tls_size -Wl,--export=__tls_align

SRC = main.c
OUT_DIR = out
WASM_OUT = $(OUT_DIR)/wasm.wasm
JS_OUT = $(OUT_DIR)/main.mjs
MAIN_WORKER_OUT = $(OUT_DIR)/main_worker.mjs
WORKER_OUT = $(OUT_DIR)/worker.mjs

all: $(OUT_DIR) $(WASM_OUT) $(JS_OUT) $(MAIN_WORKER_OUT) $(WORKER_OUT)

$(OUT_DIR):
	powershell -Command "if (!(Test-Path $(OUT_DIR))) { New-Item -ItemType Directory -Path $(OUT_DIR) }"

$(WASM_OUT): $(SRC) os/os_wasm.c
	$(CC) $(CFLAGS) $(SRC) -o $(WASM_OUT) $(LDFLAGS)

$(JS_OUT): main.ts
	bun build main.ts --outfile $(JS_OUT)

$(MAIN_WORKER_OUT): main_worker.ts
	bun build main_worker.ts --outfile $(MAIN_WORKER_OUT)

$(WORKER_OUT): worker.ts
	bun build worker.ts --outfile $(WORKER_OUT)

run: all
	bun run main.ts

clean:
	powershell -Command "if (Test-Path $(OUT_DIR)) { Remove-Item -Recurse -Force $(OUT_DIR) }"

.PHONY: all clean run
