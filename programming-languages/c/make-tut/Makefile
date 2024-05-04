OPT=-O0
CFLAGS = -Wall -Wextra -Wpedantic -Werror $(OPT)
CC = gcc

OBJECTS=main.o hello.o
BINARY=output.exe

$(BINARY): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	del /Q *.o preferred-name.exe
