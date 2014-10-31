#include <stdlib.h>
#include <semaphore.h>
#include <ucontext.h>
#include <assert.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "lib/heap.h"

#include "uthread.h"


/* Define custom data structures. ************************************************/
typedef struct {
	ucontext_t context;
	long running_time;
} uthread_record_t;


int long_cmp(long a, long b) {
	return (a > b) - (a < b);
}

/**
 * Interprets `key1` and `key2` as pointers to `uthread_record_t` objects, and
 * compares them. The comparison is based on the running time of the two records.
 * In particular, given the two records, the record with the smaller running time will
 * have the greater priority.
 */
int uthread_record_priority(const void* key1, const void* key2)
{
	const uthread_record_t* rec1 = key1;
	const uthread_record_t* rec2 = key2;

	int cmp = long_cmp(rec1->running_time, rec2->running_time);
	return -cmp;
}


/* Define file-global variables. *************************************************/

Heap _waiting_threads = NULL;
int _max_number_of_klt;
struct timeval* thread_start_times;
sem_t _mutex;



/* Define primary functions. *****************************************************/

void system_init(int max_number_of_klt) {
	uthread_init(max_number_of_klt);
}

void uthread_init(int max_number_of_klt)
{
	// To prevent this function from being called twice:
	assert(_waiting_threads == NULL);

	// The highest priority uthread record (i.e. the on with the lowest running time)
	// will be at top of the `heap`. Thus, the heap is bottom-heavy w.r.t. running time.
	_waiting_threads = HEAPinit(uthread_record_priority, NULL);

	// Initialize other globals.
	_max_number_of_klt = max_number_of_klt;
	thread_start_times = malloc(_max_number_of_klt * sizeof(struct timeval));
	sem_init(&_mutex, 0, 1);
}

int uthread_create(void (*func)())
{
	sem_wait(&_mutex);
	// TODO
	sem_post(&_mutex);
}

void uthread_yield()
{
	sem_wait(&_mutex);
	// TODO
	sem_post(&_mutex);
}

void uthread_exit()
{
	sem_wait(&_mutex);
	// TODO
	sem_post(&_mutex);
}



/* Define helper functions. ******************************************************/
