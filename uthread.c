#define _GNU_SOURCE
#include <stdlib.h>
#include <stdbool.h>
#include <ucontext.h>
#include <assert.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <sys/syscall.h>

#include "lib/heap.h"
#include "lib/tvhelp.h"

#include "uthread.h"


/* Define private directives. ****************************************************/

#define UCONTEXT_STACK_SIZE  16384
#define CLONE_STACK_SIZE     16384
#define MAX_NUM_UTHREADS     1000
#define gettid()             (syscall(SYS_gettid))

/* Define custom data structures. ************************************************/

typedef struct {
	void* fst;
	void* snd;
} ptrpair_t;

typedef struct {
	ucontext_t ucontext;
	struct timeval running_time;
	bool active;
} uthread_t;


typedef struct {
	int tid;
	struct timeval initial_utime;
	struct timeval initial_stime;
	bool active;
} kthread_t;



/* Declare helper functions. *****************************************************/

int uthread_priority(const void* key1, const void* key2);
void kthread_init(kthread_t* kt);
void uthread_init(uthread_t* ut, void (*run_func)());
int kthread_runner(void* ptr);
int kthread_create(kthread_t* kt, uthread_t* ut);
void kthread_handoff(uthread_t* load_from, uthread_t* save_to);
kthread_t* find_inactive_kthread();
uthread_t* find_inactive_uthread();




/* Define file-global variables. *************************************************/

Heap _waiting_uthreads = NULL;
int _num_kthreads;
int _max_num_kthreads;
int _num_uthreads;
uthread_t* _uthreads;
kthread_t* _kthreads;
pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_attr_t* _default_pthread_attr = NULL;
ucontext_t _system_initializer_context;



/* Define primary public functions. **********************************************/

void system_init(int max_num_kthreads) {
	uthread_system_init(max_num_kthreads);
}


void uthread_system_init(int max_num_kthreads)
{
	assert(1 <= max_num_kthreads && max_num_kthreads <= MAX_NUM_UTHREADS);
	assert(_waiting_uthreads == NULL);  // Function must only be called once.

	getcontext(&_system_initializer_context);

	// The highest priority uthread record (i.e. the on with the lowest running time)
	// will be at top of the `heap`. Thus, the heap is bottom-heavy w.r.t. running time.
	_waiting_uthreads = HEAPinit(uthread_priority, NULL);

	// Allocate memory for each `kthread_t` and mark each as inactive.
	_kthreads = malloc(_max_num_kthreads * sizeof(kthread_t));
	for (int i = 0; i < _max_num_kthreads; i++) {
		_kthreads[i].active = false;
	}

	// Allocate memory for each `uthread_t` and mark each as inactive.
	_uthreads = malloc(MAX_NUM_UTHREADS * sizeof(uthread_t));
	for (int i = 0; i < MAX_NUM_UTHREADS; i++) {
		_uthreads[i].active = false;
	}

	// Initialize other globals.
	_num_uthreads = 0;
	_num_kthreads = 0;
	_max_num_kthreads = max_num_kthreads;
	// TODO: initialize `_default_pthread_attr()`.
}



int uthread_create(void (*run_func)())
{
	int rv;

	puts("uthread_create()");
	pthread_mutex_lock(&_mutex);

	_num_uthreads += 1;

	assert(_num_uthreads < MAX_NUM_UTHREADS);
	assert(_num_kthreads <= _max_num_kthreads);

	uthread_t* uthread = find_inactive_uthread();
	assert (uthread != NULL);
	uthread_init(uthread, run_func);

	if (_num_kthreads == _max_num_kthreads)
	{
		puts("Adding new `uthread` to heap.");
		// Add the new uthread record to the heap.
		HEAPinsert(_waiting_uthreads, (const void *) uthread);
	}
	else
	{
		puts("Starting `uthread` on new `kthread`.");
		// Make a pthread to run this function immediately.

		assert(HEAPsize(_waiting_uthreads) == 0);  // There must not be waiting
												   // uthreads if `_num_kthreads` is
												   // less than `_max_num_kthreads`.

		kthread_t* kthread = find_inactive_kthread();
		assert(kthread != NULL);  // There must be an inactive `kthread` if
								  // `_num_kthreads` is less than `_max_num_kthreads`.

		rv = kthread_create(kthread, uthread);
		_num_kthreads += 1;
	}

	pthread_mutex_unlock(&_mutex);
	return rv;
}




void uthread_yield()
{
	printf("uthread_yield()\n");
	pthread_mutex_lock(&_mutex);

	if (HEAPsize(_waiting_uthreads) > 0)
	{
		// TODO: fix this crazy use of memory!
		uthread_t* save_to = find_inactive_uthread();
		save_to->active = true;

		uthread_t* load_from = NULL;
		HEAPextract(_waiting_uthreads, (void **) &load_from);
		HEAPinsert(_waiting_uthreads, (void *) save_to);

		// TODO: fix this crazy possibility of race conditions!
		pthread_mutex_unlock(&_mutex);
		kthread_handoff(load_from, save_to);
	}

}


void uthread_exit()
{
	printf("uthread_exit()\n");
	pthread_mutex_lock(&_mutex);

	// Check if a uthread can use this kthread. If so, pop the uthread from the
	// heap and use this kthread. Else, destroy the kthread.
	if (HEAPsize(_waiting_uthreads) > 0)
	{
		// TODO: fix this crazy use of memory!
		uthread_t* load_from = NULL;
		HEAPextract(_waiting_uthreads, (void **) &load_from);

		// TODO: fix this crazy possibility of race conditions!
		pthread_mutex_unlock(&_mutex);
		ucontext_t* uc = &(load_from->ucontext);
		setcontext(uc);
		uc->uc_stack.ss_sp = malloc(UCONTEXT_STACK_SIZE);
		uc->uc_stack.ss_size = UCONTEXT_STACK_SIZE;
	}
	else
	{
		pthread_mutex_unlock(&_mutex);
	}
}



/* Define primary helper functions. **********************************************/

void kthread_handoff(uthread_t* load_from, uthread_t* save_to) {
	assert(load_from != NULL && save_to != NULL);
	swapcontext(&(save_to->ucontext), &(load_from->ucontext));
}

void uthread_init(uthread_t* uthread, void (*run_func)())
{
	assert(uthread != NULL);
	assert(run_func != NULL);

	// Initialize the `ucontext`.
	ucontext_t* uc = &(uthread->ucontext);
	*uc = _system_initializer_context;
	uc->uc_stack.ss_sp = malloc(UCONTEXT_STACK_SIZE);
	uc->uc_stack.ss_size = UCONTEXT_STACK_SIZE;
	makecontext(uc, run_func, 0);

	// Initialize the running time.
	struct timeval tv = { .tv_sec = 0, .tv_usec = 0 };
	uthread->running_time = tv;

	// Set as active.
	uthread->active = true;
}



/**
 * This function is expected to be a `start_routine` for `pthread_create()`
 *
 * This interprets the given void pointer as a pointer to a `ptrpair_t`. The `fst`
 * pointer is interpreted as a `kthread_t*` and the second pointer is interpreted
 * as a `uthread_t*`. This will free the object which it is given.
 */
int kthread_runner(void* ptr)
{
	printf("kthread_runner()\n");

	ptrpair_t* pair = ptr;
	kthread_t* kt = pair->fst;
	uthread_t* ut = pair->snd;
	free(pair);

	kt->tid = gettid();
	kt->active = true;

	struct rusage ru;
	const int RUSAGE_THREAD = 1;	// TODO: Fix this hack!
	getrusage(RUSAGE_THREAD, &ru);  // Only available on linux.
	kt->initial_utime = ru.ru_utime;
	kt->initial_stime = ru.ru_stime;

	ucontext_t* uc = &(ut->ucontext);
	setcontext(uc);
	uc->uc_stack.ss_sp = malloc(UCONTEXT_STACK_SIZE);
	uc->uc_stack.ss_size = UCONTEXT_STACK_SIZE;

	assert(false);  // Execution should never reach here.
}


/**
 * Run the given user thread on the given kernel thread. The kernel thread must
 * not already be active.
 */
int kthread_create(kthread_t* kt, uthread_t* ut)
{
	// TODO: everything!
	assert(kt->active == false);

	// It is the responsibility of the newly created thread to free `pair`.
	ptrpair_t* pair = malloc(sizeof(ptrpair_t));
	pair->fst = kt;
	pair->snd = ut;

	// Try using clone instead:
	void *child_stack;
	child_stack = (void *)malloc(CLONE_STACK_SIZE);
	child_stack += CLONE_STACK_SIZE - 1;
	int pid = clone(kthread_runner, child_stack, CLONE_VM|CLONE_FILES, pair);
	assert(pid > 0);
	return pid;
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
	for (int idx = 0; idx < _max_num_kthreads; idx++) {
		if (_kthreads[idx].active == false) {
			kthread = _kthreads + idx;
			break;
		}
	}
	return kthread;
}


/**
 * Returns a pointer to a `uthread_t` slot which is which is not active. If
 * no such `uthread_t` slot exists, then `NULL` is returned.
 */
uthread_t* find_inactive_uthread()
{
	uthread_t* uthread = NULL;
	for (int idx = 0; idx < MAX_NUM_UTHREADS; idx++) {
		if (_uthreads[idx].active == false) {
			uthread = _uthreads + idx;
			break;
		}
	}
	return uthread;
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
