CC = clang
CFLAGS_DEBUG = -O0 -g -Wall -Wextra -DDEBUG
CFLAGS_RELEASE = -O2 -Wall -Wextra
IFLAGS = -I./include

LDFLAGS = -L./lib -lSDL3 \
		  -Wl,-framework,CoreMedia \
		  -Wl,-framework,CoreVideo \
		  -Wl,-framework,Cocoa \
		  -Wl,-weak_framework,UniformTypeIdentifiers \
		  -Wl,-framework,IOKit \
		  -Wl,-framework,ForceFeedback \
		  -Wl,-framework,Carbon \
		  -Wl,-framework,CoreAudio \
		  -Wl,-framework,AudioToolbox \
		  -Wl,-framework,AVFoundation \
		  -Wl,-framework,Foundation \
		  -Wl,-framework,GameController \
		  -Wl,-framework,Metal \
		  -Wl,-framework,QuartzCore \
		  -Wl,-weak_framework,CoreHaptics \
		  -lpthread -lm

TARGET = ./build/pacman
SRC_PLATFORM = main_sdl.c
SRC_GAME = game.c
GAME_DLL = ./build/game.so

build-platform-debug:
	$(CC) $(CFLAGS_DEBUG) $(IFLAGS) $(SRC_PLATFORM) -o $(TARGET) $(LDFLAGS)

build-game-debug:
	$(CC) $(CFLAGS_DEBUG) $(SRC_GAME) -o $(GAME_DLL) -shared

run-debug: build-platform-debug build-game-debug
	$(TARGET)

build-platform-release:
	$(CC) $(CFLAGS_RELEASE) $(SRC_GAME) -o $(GAME_DLL) -shared

build-game-release:
	$(CC) $(CFLAGS_RELEASE) $(SRC_GAME) -o $(TARGET)

run-release: build-platform-release
	$(TARGET)
