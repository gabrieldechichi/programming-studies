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
SRCS = game.c main_sdl.c

build-debug:
	$(CC) $(CFLAGS_DEBUG) $(IFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS)

run-debug: build-debug
	$(TARGET)

build-release:
	$(CC) $(CFLAGS_RELEASE) $(IFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS)

run-release: build-release
	$(TARGET)
