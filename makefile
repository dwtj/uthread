CC=gcc
CFLAGS=-std=gnu11 -lm -lpthread
test_uthread : test_uthread.c heap.o uthread.o
	$(CC) $(CFLAGS) -o test_uthread test_uthread.c heap.o uthread.o

heap.o : lib/heap.c
	$(CC) $(CFLAGS) -c -o $@ $<

uthread.o : uthread.c
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY : clean

clean :
	rm -f *.o
