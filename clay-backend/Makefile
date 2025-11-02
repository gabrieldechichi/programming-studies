CC = clang
SRC = src/main.c
WASM_TARGET = web/app.wasm

all: build-wasm

build:
	$(CC) --target=wasm32 \
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
