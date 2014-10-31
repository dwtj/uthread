#include <stdlib.h>
#include <stdbool.h>
#include <semaphore.h>
#include <ucontext.h>
#include <assert.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "lib/heap.h"

#include "uthread.h"


/* Define custom data structures. ************************************************/
typedef struct {
	void (*run_func)();
	ucontext_t context;
	struct timeval running_time;
} uthread_record_t;



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


int uthread_create(void (*run_func)())
{
	sem_wait(&_mutex);
	// TODO
	sem_post(&_mutex);
}


void uthread_yield()
{
	sem_wait(&_mutex);
	// TODO: everything
	/*
	struct rusage temp;
	// Identify which thread will be used.
	// It should probably be stored as a pthread attribute.
	klt_id = 0
	getrusage(RUSAGE_THREAD, &temp);
	thread_start_times[klt_id] = temp.ru_utime
	*/

	sem_post(&_mutex);
}


void uthread_exit()
{
	sem_wait(&_mutex);
	// TODO
	sem_post(&_mutex);
}



/* Define helper functions. ******************************************************/

int long_cmp(long a, long b)
{
	return (a > b) - (a < b);
}


int timeval_cmp(struct timeval fst, struct timeval snd)
{
	int cmp_sec = long_cmp(fst.tv_sec, snd.tv_sec);
	int cmp_usec = long_cmp(fst.tv_usec, snd.tv_usec);
	
	// cmp_sec has priority.
	return (cmp_sec != 0) ? cmp_sec : cmp_usec;
}


bool timeval_is_leq(struct timeval fst, struct timeval snd) {
	int cmp = timeval_cmp(fst, snd);
	return cmp <= 0;
}

struct timeval timeval_diff(struct timeval fst, struct timeval snd)
{
	assert(timeval_is_leq(fst, snd));
	
	struct timeval rv = { .tv_sec  = snd.tv_sec  - fst.tv_sec,
	                      .tv_usec = snd.tv_usec - fst.tv_usec };
	return rv;
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

	int cmp = timeval_cmp(rec1->running_time, rec2->running_time);
	return -cmp;
}


bool _is_sleeping_uthread(uthread_record_t rec) {
	return rec.run_func == NULL;
}


bool _is_new_uthread(uthread_record_t rec) {
	return !_is_sleeping_uthread(rec);
}


uthread_record_t _new_uthread(void (*run_func)())
{
	uthread_record_t rv = { .run_func = run_func };
	return rv;
}


uthread_record_t _sleeping_uthread()
{
	uthread_record_t rv = { .run_func = NULL, .running_time = 0 };
	return rv;
}
