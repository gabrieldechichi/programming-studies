# Platform-specific compiler and flags
ifeq ($(OS),Windows_NT)
    CC = cl
    CFLAGS = /W0 /DCOMPILER_MSVC /D_WIN32 /Zc:preprocessor
    CFLAGS_DEBUG = /Od /Zi /DDEBUG
    CFLAGS_RELEASE = /O2
    INCLUDE_FLAGS = /Isrc /Ios /Imeta /I./ /Igenerated
    OUT_FLAG = /Fe:
    DEFINE_FLAG = /D
    LINK_FLAGS =
else
    CC = clang
    CFLAGS = -Wall -Wextra -std=c99 -DCOMPILER_CLANG
    CFLAGS_DEBUG = -O0 -g -DDEBUG
    CFLAGS_RELEASE = -O2
    INCLUDE_FLAGS = -Isrc -Ios -Imeta -I./ -Igenerated
    OUT_FLAG = -o
    DEFINE_FLAG = -D
    LINK_FLAGS = -lpthread -lm
endif

TARGET = multicore.exe
SRC = src/main.c

all: $(TARGET)

build:
	$(CC) $(INCLUDE_FLAGS) $(CFLAGS) $(CFLAGS_DEBUG) $(LINK_FLAGS) $(OUT_FLAG)$(TARGET) $(SRC)

build-release:
	$(CC) $(INCLUDE_FLAGS) $(CFLAGS) $(CFLAGS_RELEASE) $(LINK_FLAGS) $(OUT_FLAG)$(TARGET) $(SRC)

build-meta:
	$(CC) $(INCLUDE_FLAGS) $(CFLAGS) $(CFLAGS_DEBUG) $(LINK_FLAGS) $(OUT_FLAG)./out/meta/meta meta/meta.c

meta: build-meta
	./out/meta/meta

test:
	$(CC) $(INCLUDE_FLAGS) $(CFLAGS) $(CFLAGS_DEBUG) $(DEFINE_FLAG)TESTS $(LINK_FLAGS) $(OUT_FLAG)./out/test/test tests/test_main.c
	./out/test/test

clean:
	rm -f $(TARGET)

run: build
	./$(TARGET)

.PHONY: all clean run meta test
