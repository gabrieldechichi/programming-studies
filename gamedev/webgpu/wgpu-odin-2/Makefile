GAME_NAME=game
SRC=native
TARGET_WEB=web/resources/$(GAME_NAME).wasm
TARGET=web/resources/pacman
DEBUGFLAGS=-debug -o:none
ODIN_COLLECTIONS=-collection:lib=./native

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
	odin build $(SRC) -o:aggressive -out:$(TARGET_WEB) -target:js_wasm32 $(ODIN_COLLECTIONS)

run-web: build-debug-wasm
	npm run dev --prefix ./web
