CC = clang
CFLAGS = -x objective-c -framework Cocoa -framework QuartzCore -framework Metal -framework MetalKit
TARGET = clear-sapp
SOURCE = clear-sapp.c

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(SOURCE) -o $(TARGET)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: clean run