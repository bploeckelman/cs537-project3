CC=gcc
FLAGS=-c -ggdb3 --std=gnu99 -Wall #-Werror
TAGS=ctags -R

raidsim: main.o disk.o disk-array.o
	$(CC) main.o disk.o disk-array.o -o raidsim 
	$(TAGS)

test: test.o disk.o disk-array.o
	$(CC) test.o disk.o disk-array.o -o test
	$(TAGS)

main.o: main.c
	$(CC) $(FLAGS) main.c -o main.o

disk.o: disk.c
	$(CC) $(FLAGS) disk.c -o disk.o

disk-array.o: disk-array.c
	$(CC) $(FLAGS) disk-array.c -o disk-array.o

test.o: test.c
	$(CC) $(FLAGS) test.c -o test.o

clean:
	rm -f *.o raidsim test myvirtualdisk-* disk-array-*

