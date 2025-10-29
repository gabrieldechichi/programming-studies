CC = clang
CFLAGS = -Wall -Wextra -std=c99 -DCOMPILER_CLANG -DMACOS
CFLAGS_DEBUG = -O0 -g -DDEBUG
CFLAGS_RELEASE = -O2
INCLUDE_FLAGS = -Isrc -Ios -Imeta -I./
LINK_FLAGS = -lpthread -lm

TARGET = multicore
SRC = src/main.c

all: $(TARGET)

build:
	$(CC) $(INCLUDE_FLAGS) $(CFLAGS) $(CFLAGS_DEBUG) $(LINK_FLAGS) -o $(TARGET) $(SRC)

build-release:
	$(CC) $(CFLAGS) $(CFLAGS_RELEASE) $(LINK_FLAGS) -o $(TARGET) $(SRC)

build-meta:
	$(CC) $(INCLUDE_FLAGS) $(CFLAGS) $(CFLAGS_DEBUG) $(LINK_FLAGS) -o ./out/meta/meta meta/meta.c

meta: build-meta
	./out/meta/meta

test:
	$(CC) $(INCLUDE_FLAGS) $(CFLAGS) $(CFLAGS_DEBUG) -DTESTS $(LINK_FLAGS) -o ./out/test/test tests/test_main.c
	time ./out/test/test

clean:
	rm -f $(TARGET)

run: build
	time ./$(TARGET)

.PHONY: all clean run meta test
