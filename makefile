test_uthread : test_uthread.c heap.o uthread.o
	cc -o test_uthread test_uthread.c heap.o uthread.o

heap.o : lib/heap.c
	cc -c -o $@ $<

uthread.o : uthread.c
	cc -c -o $@ $<

.PHONY : clean

clean :
	rm -f *.o
