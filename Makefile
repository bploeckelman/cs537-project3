CC=gcc
FLAGS=-c -ggdb3 --std=gnu99 -Wall #-Werror
TAGS=ctags -R

raidsim: main.o disk.o disk-array.o
	$(CC) main.o disk.o disk-array.o -o raidsim 
	$(TAGS)

main.o: main.c
	$(CC) $(FLAGS) main.c -o main.o

disk.o: disk.c
	$(CC) $(FLAGS) disk.c -o disk.o

disk-array.o: disk-array.c
	$(CC) $(FLAGS) disk-array.c -o disk-array.o

clean:
	rm -f *.o raidsim 

