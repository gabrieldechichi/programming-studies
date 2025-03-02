CC = gcc
CFLAGS_DEBUG = -O0 -g -Wall -Wextra -DDEBUG
CFLAGS_RELEASE = -O2 -Wall -Wextra
IFLAGS=-I./include
LDFLAGS = -L./lib -lSDL3.0 -Wl,-rpath,@executable_path/lib -Wl,-rpath,@executable_path/../lib

TARGET = ./build/pacman
SRCS = main2.c

build-debug:
	$(CC) $(CFLAGS_DEBUG) $(IFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS)

run-debug: build-debug
	$(TARGET)

build-release:
	$(CC) $(CFLAGS_RELEASE) $(IFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS)

run-release: build-release
	$(TARGET)
