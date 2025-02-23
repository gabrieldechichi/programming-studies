CC = gcc
CFLAGS_DEBUG = -O0 -g -Wall -DDEBUG
CFLAGS_RELEASE = -O2 -Wall

SOURCE = ./main.c
TARGET = ./build/pacman

INCLUDE_RAYLIB = -Iraylib/include
BUILD_RAYLIB = raylib/lib/libraylib.a -framework Cocoa -framework IOKit

build-debug:
	$(CC) $(CFLAGS_DEBUG) $(SOURCE) -o $(TARGET) $(INCLUDE_RAYLIB) $(BUILD_RAYLIB)

run-debug: build-debug
	$(TARGET)

build-release:
	$(CC) $(CFLAGS_RELEASE) $(SOURCE) -o $(TARGET) $(INCLUDE_RAYLIB) $(BUILD_RAYLIB)

run-release: build-release
	$(TARGET)
