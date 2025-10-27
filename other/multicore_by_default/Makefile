CC = clang
CFLAGS = -Wall -Wextra -std=c99
CFLAGS_DEBUG = -O0
LINK_FLAGS = -lpthread -lm

TARGET = multicore
SRC = main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(CFLAGS_DEBUG) $(LINK_FLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	time ./$(TARGET)

.PHONY: all clean run
