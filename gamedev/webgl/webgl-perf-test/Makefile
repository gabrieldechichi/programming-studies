GAME_NAME=game
SRC=native
TARGET_WEB=web/public/$(GAME_NAME).wasm
TARGET=web/public
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

publish-clean:
	rm -rf game
	rm -rf game.zip

publish-wasm: publish-clean build-release-wasm
	npm run build --prefix ./web
	rm -f game.zip
	cd web && find src -name "*.js" | zip ../game.zip -@
	cd web/dist && zip -r ../../game.zip . 

publish-play: publish-wasm
	unzip game.zip -d game
	http-server ./game -p 8800
