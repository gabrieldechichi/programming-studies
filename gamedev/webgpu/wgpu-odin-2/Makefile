
GAME_NAME=game
SRC=native
TARGET_WEB=web/resources/$(GAME_NAME).wasm
TARGET=web/resources/pacman
DEBUGFLAGS=-debug -o:none
ODIN_COLLECTIONS=-collection:lib=./native


INITIAL_MEMORY_PAGES := 2000
MAX_MEMORY_PAGES     := 65536

PAGE_SIZE := 65536
INITIAL_MEMORY_BYTES := 131072000
MAX_MEMORY_BYTES     := 4294967296

# Detect OS
ifeq ($(OS),Windows_NT)
  MKDIR_EXEC = powershell -Command "if (-Not (Test-Path '$(TARGET)')) { New-Item -ItemType Directory -Path '$(TARGET)'; }"
else
  MKDIR_EXEC = mkdir -p $(TARGET)
endif

odin-check:
	odin check $(SRC) -target:js_wasm32 $(ODIN_COLLECTIONS)

build-debug-wasm:
	$(MKDIR_EXEC)
	odin build $(SRC) -out:$(TARGET_WEB) -target:js_wasm32 $(ODIN_COLLECTIONS)

build-release-wasm:
	$(MKDIR_EXEC)
	odin build $(SRC) -out:$(TARGET_WEB) $(ODIN_COLLECTIONS) \
		-target:js_wasm32 -o:aggressive \
        -extra-linker-flags:"--export-table --import-memory --initial-memory=$(INITIAL_MEMORY_BYTES) --max-memory=$(MAX_MEMORY_BYTES)"

run-web: build-release-wasm
	npm run dev --prefix ./web

build-native:
	odin build $(SRC) $(ODIN_COLLECTIONS) -debug

run-native:
	odin run $(SRC) $(ODIN_COLLECTIONS) -debug
	
