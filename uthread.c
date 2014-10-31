#include <stdlib.h>

#include "lib/heap.h"


/* Define custom data structures. ************************************************/
typedef struct {
	// TODO: save context here
	long running_time;
} uthread_record_t;


int long_cmp(long a, long b) {
	return (a > b) - (a < b);
}

int uthread_record_cmp(const void* key1, const void* key2) {
	const uthread_record_t* rec1 = key1;
	const uthread_record_t* rec2 = key2;

	return long_cmp(rec1->running_time, rec2->running_time);
}


/* Define file-global variables. *************************************************/
Heap waiting_threads;

void system_init(int max_number_of_klt)
{
	waiting_threads = HEAPinit(uthread_record_cmp, NULL);
}

int uthread_create(void (*func)())
{
	// TODO
}

void uthread_yield()
{
	// TODO
}

void uthread_exit()
{
	// TODO
}
