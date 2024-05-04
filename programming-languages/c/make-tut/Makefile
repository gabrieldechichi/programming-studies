CFLAGS = -Wall

output.exe: main.o
	gcc $(CFLAGS) main.o -o output.exe

main.o: main.c hello.h
	gcc $(CFLAGS) -c main.c

clean:
	del /Q *.o preferred-name.exe
