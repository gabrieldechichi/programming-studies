CC = gcc
OPT=-O0
TARGET = bin/main
CFLAGS = -Wall -Wextra -g $(OPT)
INCDIRS = . include
LINK=-lglfw -lGL -lleif -lclipboard -lm -lxcb

build:
	$(CC) $(CFLAGS) main.c $(LINK) -o $(TARGET)

run: build
	$(TARGET)
