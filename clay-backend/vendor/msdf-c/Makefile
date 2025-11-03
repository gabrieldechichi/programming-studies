CC = gcc
FLAGS = -g -lm -std=c99 -O2

all: msdf atlas_gen

msdf: main.o msdf.o
	$(CC) main.o msdf.o -o msdf_gen $(FLAGS)

atlas_gen: main_atlas.o msdf.o atlas_packer.o
	$(CC) main_atlas.o msdf.o atlas_packer.o -o atlas_gen $(FLAGS)

main.o: main.c msdf.h
	$(CC) -c main.c $(FLAGS)

main_atlas.o: main_atlas.c msdf.h atlas_packer.h
	$(CC) -c main_atlas.c $(FLAGS)

msdf.o: msdf.c msdf.h
	$(CC) -c msdf.c $(FLAGS)

atlas_packer.o: atlas_packer.c atlas_packer.h
	$(CC) -c atlas_packer.c $(FLAGS)

clean:
	rm -f *.o msdf_gen atlas_gen