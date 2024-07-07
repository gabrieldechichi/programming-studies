GAME_NAME=game
SRC=native
TARGET_WEB=web/resources/$(GAME_NAME).wasm
TARGET=web/resources/pacman
DEBUGFLAGS=-debug -o:none

build-debug-wasm:
	mkdir -p $(TARGET)
	odin build $(SRC) -out:$(TARGET_WEB) -target:js_wasm32

run-web: build-debug-wasm
	npm run dev --prefix ./web 


