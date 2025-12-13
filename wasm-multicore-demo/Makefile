CC = clang
CFLAGS = --target=wasm32-wasi-threads \
		 --sysroot ./wasi-sdk/win32/share/wasi-sysroot -msimd128 \
		 -DCGLM_SIMD -DCGLM_SIMD_WASM \
		 -O2 -I. -DWASM -matomics -mbulk-memory

# --shared-memory requires --max-memory (shared memory can't grow)
# --export-table needed for indirect function calls (function pointers) from JS
# --export TLS symbols for thread-local storage support
LDFLAGS = -Wl,--no-entry -Wl,--export-table -Wl,--import-memory \
		  -Wl,--shared-memory -Wl,--max-memory=4294967296 \
		  -Wl,--allow-undefined -Wl,--export=__wasm_init_tls \
		  -Wl,--export=__tls_size -Wl,--export=__tls_align \
		  --sysroot ./wasi-sdk/win32/share/wasi-sysroot \
		  -resource-dir ./wasi-sdk/win32/lib/clang/21

SRC = main.c
OUT_DIR = out

all: dirs wasm js

dirs:
	powershell -Command "if (!(Test-Path $(OUT_DIR))) { New-Item -ItemType Directory -Path $(OUT_DIR) }"

wasm:
	$(CC) $(CFLAGS) $(SRC) -o $(OUT_DIR)/wasm.wasm $(LDFLAGS)

js:
	bun build main.ts --outfile $(OUT_DIR)/main.mjs
	bun build main_worker.ts --outfile $(OUT_DIR)/main_worker.mjs
	bun build worker.ts --outfile $(OUT_DIR)/worker.mjs

run: all
	bun run main.ts

clean:
	powershell -Command "if (Test-Path $(OUT_DIR)) { Remove-Item -Recurse -Force $(OUT_DIR) }"

.PHONY: all dirs wasm js clean run
