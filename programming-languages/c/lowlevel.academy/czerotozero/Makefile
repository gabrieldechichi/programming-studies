TARGET = bin/final
SRC = $(wildcard src/*.c)
OBJ = $(patsubst src/%.c, obj/%.o, $(SRC))
INCDIRS = . include
CC = gcc
OPT=-O0
CFLAGS=-Wall -Wextra $(OPT) $(foreach D,$(INCDIRS),-I$(D)) -g

.PHONY: all clean test

default: $(TARGET)

obj/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $?

clean:
	rm -f bin/*
	rm -f obj/*.o
	rm -f test/bin/*
	rm -f test/obj/*

# TESTS
TESTSRC = $(wildcard test/*.c)
TESTOBJ = $(patsubst test/%.c,test/obj/%.o,$(TESTSRC))
TESTBIN = $(patsubst test/%.c,test/bin/%,$(TESTSRC))
TESTOBJ = $(filter-out obj/main.o,$(OBJ))
UNITYSRC = ./vendor/Unity/src/unity.c
UNITYOBJ = ./test/obj/unity.o

test: test-build
	@echo Running tests...
	@$(foreach t,$(TESTBIN),./$(t);)

test-build: $(TESTBIN)

test/bin/%: test/obj/%.o $(UNITYOBJ) $(TESTOBJ)
	$(CC) $(CFLAGS) -o $@ $^

test/obj/%.o: test/%.c
	$(CC) $(CFLAGS) -Ivendor/Unity/src -c ./vendor/Unity/src/unity.c  -o ./test/obj/unity.o
	$(CC) $(CFLAGS) -Ivendor/Unity/src -c $< -o $@

$(UNITYOBJ): $(UNITYSRC)
	$(CC) $(CFLAGS) -Ivendor/Unity/src -c $< -o $@
