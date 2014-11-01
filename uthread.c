#include <stdlib.h>
#include <stdbool.h>
#include <ucontext.h>
#include <assert.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <pthread.h>

#include "lib/heap.h"

#include "uthread.h"


/* Define private directives. ****************************************************/

#define UCONTEXT_STACK_SIZE 16384


/* Define custom data structures. ************************************************/

typedef struct {
	void* fst;
	void* snd;
} ptrpair_t;

typedef struct {
	ucontext_t ucontext;
	struct timeval running_time;
} uthread_t;


typedef struct {
	pthread_t pthread;
	struct timeval initial_utime;
	struct timeval initial_stime;
	bool active;
} kthread_t;



/* Declare helper functions. *****************************************************/

int uthread_priority(const void* key1, const void* key2);
void kthread_init(kthread_t* kt);
void uthread_init(uthread_t* ut, void (*run_func)());
void* kthread_runner(void* ptr);
void kthread_create(kthread_t* kt, uthread_t* ut);
kthread_t* find_inactive_kthread();




/* Define file-global variables. *************************************************/

Heap _waiting_uthreads = NULL;
int _num_klt;
int _max_num_klt;
kthread_t* _kthreads;
pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_attr_t* _default_pthread_attr = NULL;
ucontext_t _creator_context;


/* Define primary public functions. **********************************************/

void system_init(int max_num_klt) {
	uthread_system_init(max_num_klt);
}


void uthread_system_init(int max_num_klt)
{
	assert(max_num_klt >= 1);
	assert(_waiting_uthreads == NULL);  // Function must only be called once.

	// The highest priority uthread record (i.e. the on with the lowest running time)
	// will be at top of the `heap`. Thus, the heap is bottom-heavy w.r.t. running time.
	_waiting_uthreads = HEAPinit(uthread_priority, NULL);

	// Allocate memory for each `kthread_t` and initialize each.
	_kthreads = malloc(_max_num_klt * sizeof(kthread_t));
	for (int i = 0; i < _max_num_klt; i++) {
		kthread_init(_kthreads + i);
	}

	// Initialize other globals.
	_num_klt = 0;
	_max_num_klt = max_num_klt;
	// TODO: initialize `_default_pthread_attr()`.
}



int uthread_create(void (*run_func)())
{
	pthread_mutex_lock(&_mutex);

	if (_num_klt < _max_num_klt)
	{
		// Make a pthread to run this function immediately.

		assert(HEAPsize(_waiting_uthreads) == 0);  // There must not be waiting
												   // uthreads if `_num_klt` is
												   // less than `_max_num_klt`.

		kthread_t* kthread = find_inactive_kthread();
		assert(kthread != NULL);  // There must be an inactive `kthread` if
								  // `_num_klt` is less than `_max_num_klt`.

		uthread_t uthread;
		uthread_init(&uthread, run_func);
		kthread_create(kthread, &uthread);

	}
	else
	{
		// Add the new uthread record to the heap.
		uthread_t* utp = malloc(sizeof(uthread_t));
		HEAPinsert(_waiting_uthreads, (const void *) utp);
	}

	pthread_mutex_unlock(&_mutex);
}




void uthread_yield()
{
	pthread_mutex_lock(&_mutex);

	assert(false);  // TODO: not implemented error

	pthread_mutex_unlock(&_mutex);
}


void uthread_exit()
{
	pthread_mutex_lock(&_mutex);
	// Check if a uthread can use this kthread. If so, pop the uthread from the
	// heap and use this kthread. Else, destroy the kthread.

	assert(false);  // TODO: not implemented error

	pthread_mutex_unlock(&_mutex);
}



/* Define primary helper functions. **********************************************/

void uthread_init(uthread_t* uthread, void (*run_func)())
{
	// Initialize the `ucontext`.
	ucontext_t* ucp = &(uthread->ucontext);
	getcontext(ucp);
	ucp->uc_stack.ss_sp = malloc(UCONTEXT_STACK_SIZE);
	ucp->uc_stack.ss_size = UCONTEXT_STACK_SIZE;
	makecontext(ucp, run_func, 0);

	// Initialize the running time.
	struct timeval tv = { .tv_sec = 0, .tv_usec = 0 };
	uthread->running_time = tv;
}



/**
 * This function is expected to be a `start_routine` for `pthread_create()`
 *
 * This interprets the given void pointer as a pointer to a `ptrpair_t`. The `fst`
 * pointer is interpreted as a `kthread_t*` and the second pointer is interpreted
 * as a `uthread_t*`. This will free the object which it is given.
 */
void* kthread_runner(void* ptr)
{
	ptrpair_t* pair = ptr;
	kthread_t* kt = pair->fst;
	uthread_t* ut = pair->snd;
	free(pair);

	struct rusage ru;
	const int RUSAGE_THREAD = 1;	// TODO: Fix this hack!
	getrusage(RUSAGE_THREAD, &ru);  // Only available on linux.
	kt->initial_utime = ru.ru_utime;
	kt->initial_stime = ru.ru_stime;

	setcontext(&(ut->ucontext));
}


/**
 * Run the given user thread on the given kernel thread. The kernel thread must
 * not already be active.
 */
void kthread_create(kthread_t* kt, uthread_t* ut)
{
	// TODO: everything!
	assert(kt->active == false);

	ptrpair_t* pair = malloc(sizeof(ptrpair_t));
	int err = pthread_create(&(kt->pthread), _default_pthread_attr,
							 kthread_runner, (void*) pair);
	assert (err == 0);  // Cannot handle `pthread` creation errors.
}




/* Define minor helper functions. ************************************************/

void kthread_init(kthread_t* kt) {
	kt->active = false;
}


/**
 * Returns a pointer to a `kthread_t` slot which is which is not active. If
 * no such `kthread_t` slot exists, then `NULL` is returned.
 */
kthread_t* find_inactive_kthread()
{
	kthread_t* kthread = NULL;
	for (int idx = 0; idx < _max_num_klt; idx++) {
		if (_kthreads[idx].active == false) {
			kthread = _kthreads + idx;
			break;
		}
	}
	return kthread;
}


/**
 * Interprets `key1` and `key2` as pointers to `uthread_t` objects, and
 * compares them. The comparison is based on the running time of the two records.
 * In particular, given the two records, the record with the smaller running time will
 * have the greater priority.
 */
int uthread_priority(const void* key1, const void* key2)
{
	const uthread_t* rec1 = key1;
	const uthread_t* rec2 = key2;

	int cmp = timeval_cmp(rec1->running_time, rec2->running_time);
	return -cmp;
}




/* Define `timeval` helper functions. ********************************************/

int long_cmp(long a, long b)
{
	return (a > b) - (a < b);
}


struct timeval timeval_add(struct timeval fst, struct timeval snd)
{
	long usec = fst.tv_usec + snd.tv_usec;
	long sec = fst.tv_sec + snd.tv_usec;

	const long NUM_USEC_IN_SEC = 1000000;
	sec += usec / NUM_USEC_IN_SEC;
	usec %= NUM_USEC_IN_SEC;
	
	struct timeval rv = { .tv_sec = sec, .tv_usec = usec };
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
