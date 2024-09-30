CC = gcc
CFLAGS = -Wall -fsanitize=address -std=c99 -O2

all: mysh clean

arraylist.o: arraylist.c arraylist.h
	$(CC) $(CFLAGS) -c -Wall arraylist.c

mysh.o: mysh.c arraylist.h
	$(CC) $(CFLAGS)	-c mysh.c

mysh: mysh.o arraylist.o
	$(CC) $(CFLAGS) mysh.o arraylist.o -o mysh

clean: 
	rm *.o
