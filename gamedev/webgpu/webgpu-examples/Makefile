CC = gcc
OPT=-O0
TARGET = bin/main
CFLAGS = -Wall -Wextra -g $(OPT) $(addprefix -I, $(INCDIRS)) -std=c99 -pedantic
INCDIRS = . include src ./vendor ./vendor/stb ./vendor/webgpu/ ./vendor/cglm/include vendor/glfw3webgpu 
LINK=-lglfw -L./vendor/webgpu/ -lwgpu_native -L./vendor/cglm/build/ -lcglm -lm
RPATH=-Wl,-rpath=./vendor/webgpu

build:
	$(CC) $(CFLAGS) unity.c $(LINK) $(RPATH) -o $(TARGET)

run: build
	$(TARGET)
