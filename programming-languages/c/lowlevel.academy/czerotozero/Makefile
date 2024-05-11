TARGET = bin/final
SRC = $(wildcard src/*.c)
OBJ = $(patsubst src/%.c, obj/%.o, $(SRC))
INCDIRS = . include
CC = gcc
OPT=-O0
CFLAGS=-Wall -Wextra $(OPT) $(foreach D,$(INCDIRS),-I$(D)) 

default: $(TARGET)

obj/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJ)
	gcc -o $@ $?

clean:
	rm -f bin/*
	rm -f obj/*.o
