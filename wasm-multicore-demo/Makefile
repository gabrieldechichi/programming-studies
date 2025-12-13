CC = clang

# Optimization flags:
#   -O3                    Aggressive optimization
#   -flto                  Link-time optimization (cross-function inlining)
#   -ffast-math            Faster FP ops (fine for games, not bit-exact)
#   -fno-exceptions        No C++ exceptions (not needed for C)
#   -DNDEBUG               Disable asserts
#   -fomit-frame-pointer   Free up a register
#   -fno-stack-protector   Remove stack canary overhead
#   -funroll-loops         Loop unrolling
#   -ffunction-sections    Enable dead code elimination
#   -fdata-sections        Enable unused data elimination
#
# WASM-specific:
#   -msimd128              128-bit SIMD
#   -matomics              Atomics for threading
#   -mbulk-memory          Bulk memory ops (memcpy/memset)
#   -mnontrapping-fptoint  Faster float→int (no traps on overflow)
#   -msign-ext             Sign extension instructions
#   -mrelaxed-simd         Relaxed SIMD (faster, less deterministic)

CFLAGS = --target=wasm32-wasi-threads \
		 --sysroot ./wasi-sdk/win32/share/wasi-sysroot \
		 -O3 -flto -DNDEBUG \
		 -ffast-math \
		 -fomit-frame-pointer -fno-stack-protector -funroll-loops \
		 -ffunction-sections -fdata-sections \
		 -msimd128 -matomics -mbulk-memory \
		 -DCGLM_SIMD -DCGLM_SIMD_WASM \
		 -I. -DWASM

# CFLAGS = --target=wasm32-wasi-threads \
# 		 --sysroot ./wasi-sdk/win32/share/wasi-sysroot \
# 		 -O0 -g -DDEBUG \
# 		 -ffast-math \
# 		 -fomit-frame-pointer -fno-stack-protector -funroll-loops \
# 		 -ffunction-sections -fdata-sections \
# 		 -msimd128 -matomics -mbulk-memory \
# 		 -DCGLM_SIMD -DCGLM_SIMD_WASM \
# 		 -I. -DWASM

# --shared-memory requires --max-memory (shared memory can't grow)
# --export-table needed for indirect function calls (function pointers) from JS
# --export TLS symbols for thread-local storage support
# --gc-sections removes unused code/data (works with -ffunction/data-sections)
# --lto-O3 sets LTO optimization level
LDFLAGS = -Wl,--no-entry -Wl,--export-table -Wl,--import-memory \
		  -Wl,--shared-memory -Wl,--max-memory=4294967296 \
		  -Wl,--allow-undefined -Wl,--export=__wasm_init_tls \
		  -Wl,--export=__tls_size -Wl,--export=__tls_align \
		  -Wl,--gc-sections -Wl,--lto-O3 \
		  --sysroot ./wasi-sdk/win32/share/wasi-sysroot \
		  -resource-dir ./wasi-sdk/win32/lib/clang/21 \
		  -flto

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
