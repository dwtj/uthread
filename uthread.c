#include <stdlib.h>
#include <stdbool.h>
#include <ucontext.h>
#include <assert.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <pthread.h>

#include "lib/heap.h"

#include "uthread.h"


/* Define custom data structures. ************************************************/

typedef struct {
	ucontext_t context;
	struct timeval running_time;
} uthread_rec_t;


typedef struct {
	pthread_t pthread;
	struct timeval initial_utime;
	bool active;
} kthread_rec_t;



/* Declare helper functions. *****************************************************/

int _uthread_rec_priority(const void* key1, const void* key2);
//void _uthread_rec_init(context, running_time);



/* Define file-global variables. *************************************************/

Heap _waiting_uthreads = NULL;
int _num_klt;
int _max_num_klt;
kthread_rec_t* _kthreads;
pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER;



/* Define primary public functions. **********************************************/

void system_init(int max_num_klt) {
	uthread_init(max_num_klt);
}


void uthread_init(int max_num_klt)
{
	assert(_waiting_uthreads == NULL);  // Function must only be called once.

	// The highest priority uthread record (i.e. the on with the lowest running time)
	// will be at top of the `heap`. Thus, the heap is bottom-heavy w.r.t. running time.
	_waiting_uthreads = HEAPinit(_uthread_rec_priority, NULL);

	// Allocate memory for each `kthread_rec_t` and mark each of them as not active.
	_kthreads = malloc(_max_num_klt * sizeof(kthread_rec_t));
	kthread_rec_t not_active_kthread = { .active = false };
	for (int i = 0; i < _max_num_klt; i++) {
		_kthreads[i] = not_active_kthread;
	}

	// Initialize other globals.
	_num_klt = 0;
	_max_num_klt = max_num_klt;
}


int uthread_create(void (*run_func)())
{
	pthread_mutex_lock(&_mutex);

	if (_num_klt < _max_num_klt)
	{
		// Make a pthread to run this function immediately.
		assert(HEAPsize(_waiting_uthreads) == 0);

		// Find the first kthread space which is not active:
		kthread_rec_t* kthread = NULL;
		for (int idx = 0; idx < _max_num_klt; idx++) {
			if (_kthreads[idx].active == false) {
				kthread = _kthreads + idx;
				break;
			}
		}
		assert(kthread != NULL);

		assert(false);  // TODO: not implemented error

		// TODO: create uthread
		// _run_on(uthread, kthread);
	}
	else
	{
		// Add the new uthread record to the heap.
		uthread_rec_t* rec = malloc(sizeof(uthread_rec_t));
		//*rec = _uthread_rec_init();
		HEAPinsert(_waiting_uthreads, (const void *) rec);
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

/**
 * Interprets the `arg` to be a `uthread_rec_t*` and executes its `run_func()`.
 */
void* _run_uthread(void* uthread_rec) {
	uthread_rec_t* rec = uthread_rec;
	//uthread_rec->run_func();
	uthread_exit();
}


/**
 * Run the given user thread on the given kernel thread. The kernel thread must
 * not be active, and the user thread must be new.
 */
bool _run_new_uthread(uthread_rec_t* ut, kthread_rec_t* kt)
{
	assert(kt->active == false);
	// If the given kthread is not active, then make a new kthread there.
	int err = pthread_create(&(kt->pthread), NULL, _run_uthread, ut);
	assert (err == 0);  // Cannot handle `pthread` creation errors.
	
	struct rusage ru;
	const int RUSAGE_THREAD = 1;
	getrusage(RUSAGE_THREAD, &ru);  // Only available on linux.
	kt->initial_utime = ru.ru_utime;
}


bool _handoff_kthread_to(kthread_rec_t* kt, uthread_rec_t ut)
{
	assert(false);  // TODO: not implemented error
}





/* Define minor helper functions. ************************************************/

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


/**
 * Interprets `key1` and `key2` as pointers to `uthread_rec_t` objects, and
 * compares them. The comparison is based on the running time of the two records.
 * In particular, given the two records, the record with the smaller running time will
 * have the greater priority.
 */
int _uthread_rec_priority(const void* key1, const void* key2)
{
	const uthread_rec_t* rec1 = key1;
	const uthread_rec_t* rec2 = key2;

	int cmp = timeval_cmp(rec1->running_time, rec2->running_time);
	return -cmp;
}
