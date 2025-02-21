CC = gcc
CFLAGS_DEBUG = -O0 -g -Wall -Werror -DDEBUG
CFLAGS_RELEASE = -O2 -Wall -Werror

SOURCE = ./main.c
TARGET = ./build/pacman

INCLUDE_RAYLIB = -Iraylib/include
BUILD_RAYLIB = raylib/lib/libraylib.a -framework Cocoa -framework IOKit

build-debug:
	$(CC) $(CFLAGS_DEBUG) $(SOURCE) -o $(TARGET) $(INCLUDE_RAYLIB) $(BUILD_RAYLIB)

run-debug: build-debug
	$(TARGET)
