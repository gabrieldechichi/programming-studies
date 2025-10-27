CC = clang
CFLAGS = -Wall -Wextra -std=c99 -DCOMPILER_CLANG
CFLAGS_DEBUG = -O0
LINK_FLAGS = -lpthread -lm

TARGET = multicore
SRC = main.c

all: $(TARGET)

build:
	$(CC) $(CFLAGS) $(CFLAGS_DEBUG) $(LINK_FLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

run: build
	time ./$(TARGET)

.PHONY: all clean run
