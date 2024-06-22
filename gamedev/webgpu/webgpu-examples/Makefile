CC = gcc
OPT=-O0
TARGET = bin/main
CFLAGS = -Wall -Wextra -g $(OPT) $(addprefix -I, $(INCDIRS))
INCDIRS = . include src ./vendor ./vendor/stb ./vendor/webgpu/ ./vendor/glfw3webgpu 
LINK=-lglfw -L./vendor/webgpu/ -lwgpu_native
RPATH=-Wl,-rpath=./vendor/webgpu

build:
	$(CC) $(CFLAGS) unity.c $(LINK) $(RPATH) -o $(TARGET)

run: build
	$(TARGET)
