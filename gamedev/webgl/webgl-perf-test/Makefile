GAME_NAME=game
SRC=native
TARGET_WEB=web/resources/$(GAME_NAME).wasm
TARGET=web/resources/game
DEBUGFLAGS=-debug -o:none
ODIN_COLLECTIONS=-collection:lib=./native

odin-check:
	odin check $(SRC) -target:js_wasm32 $(ODIN_COLLECTIONS)

build-debug-wasm:
	mkdir -p $(TARGET)
	odin build $(SRC) -o:none -out:$(TARGET_WEB) -target:js_wasm32 $(ODIN_COLLECTIONS)

build-release-wasm:
	mkdir -p $(TARGET)
	odin build $(SRC) -o:speed -out:$(TARGET_WEB) -target:js_wasm32 $(ODIN_COLLECTIONS)

run-web: build-debug-wasm
	npm run dev --prefix ./web 
