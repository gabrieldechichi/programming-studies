PROJNAME = cint
SRC = unity.c
OUTDIR_DEBUG = out/debug
OUTDIR_RELEASE = out/release
TARGET_DEBUG = $(OUTDIR_DEBUG)/$(PROJNAME)
TARGET_RELEASE = $(OUTDIR_RELEASE)/$(PROJNAME)
CC = gcc
CFLAGS_DEBUG = -g -Wall -DDEBUG
CFLAGS_RELEASE = -O2 -Wall

# Ensure output directories exist
$(shell mkdir -p $(OUTDIR_DEBUG))
$(shell mkdir -p $(OUTDIR_RELEASE))

build-debug:
	$(CC) $(CFLAGS_DEBUG) -o $(TARGET_DEBUG) $(SRC)

build-release:
	$(CC) $(CFLAGS_RELEASE) -o $(TARGET_RELEASE) $(SRC)

run-debug: build-debug
	./$(TARGET_DEBUG)

run-release: build-release
	./$(TARGET_RELEASE)

.PHONY: build-debug build-release run-debug run-release
