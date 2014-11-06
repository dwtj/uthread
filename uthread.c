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

#include "uthread.h"


/* Define private directives. ****************************************************/

#define UCONTEXT_STACK_SIZE     16384
#define CLONE_STACK_SIZE        16384
#define MAX_NUM_UTHREADS        1000
#define gettid()                (syscall(SYS_gettid))
#define tgkill(tgid, tid, sig)  (syscall(SYS_tgkill, tgid, tid, sig))

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
	int tid;
	struct timeval utime_timestamp;
	struct timeval stime_timestamp;
	uthread_t* running;
} kthread_t;



/* Declare helper functions. *****************************************************/

int uthread_priority(const void* key1, const void* key2);
void uthread_init(uthread_t* ut, void (*run_func)());
int kthread_runner(void* ptr);
void transfer_elapsed_time(kthread_t* kt, uthread_t* ut);
void kthread_update_timestamps(kthread_t* kt);
void get_thread_rusage(struct rusage* ru);
int kthread_create(kthread_t* kt, uthread_t* ut);
void kthread_handoff(uthread_t* load_from, uthread_t* save_to);
kthread_t* kthread_self();
kthread_t* find_inactive_kthread();
bool waiting_uthread_has_priority_over(uthread_t* ut);
void uthread_print(const void* key);




/* Define file-global variables. *************************************************/

bool _shutdown = false;
Heap _waiting_uthreads = NULL;
int _num_kthreads;
int _max_num_kthreads;
kthread_t* _kthreads;
pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t _shutdown_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_attr_t* _default_pthread_attr = NULL;
ucontext_t _system_initializer_context;



/* Define primary public functions. **********************************************/

void system_init(int max_num_kthreads) {
	uthread_system_init(max_num_kthreads);
}


void uthread_system_init(int max_num_kthreads)
{
	assert(_shutdown == false);
	assert(1 <= max_num_kthreads && max_num_kthreads <= MAX_NUM_UTHREADS);
	assert(_waiting_uthreads == NULL);  // Function must only be called once.

	// Initialize some globals.
	_num_kthreads = 0;
	_max_num_kthreads = max_num_kthreads;
	// TODO: initialize `_default_pthread_attr()`.
	getcontext(&_system_initializer_context);

	// The highest priority uthread record (i.e. the on with the lowest running
	// time) will be at top of the `heap`. Thus, the heap is bottom-heavy w.r.t.
	// running time.
	_waiting_uthreads = HEAPinit(uthread_priority, NULL);

	// Allocate memory for each `kthread_t` and mark each as inactive (i.e. not
	// running).
	_kthreads = malloc(max_num_kthreads * sizeof(kthread_t));
	for (int i = 0; i < _max_num_kthreads; i++) {
		_kthreads[i].running = NULL;
	}

}


int uthread_create(void (*run_func)())
{
	int rv;

	pthread_mutex_lock(&_mutex);
	assert(_shutdown == false);

	// Lock the system from shutting down while there is a uthread running.
	if (_num_kthreads == 0) {
		pthread_mutex_lock(&_shutdown_mutex);
	}

	uthread_t* uthread = malloc(sizeof(uthread_t));
	assert (uthread != NULL);
	uthread_init(uthread, run_func);

	if (_num_kthreads == _max_num_kthreads)
	{
		// Add the new `uthread` to the heap.
		HEAPinsert(_waiting_uthreads, (const void *) uthread);
	}
	else
	{
		// Make a new `kthread` to run this new `uthread` immediately.

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
	pthread_mutex_lock(&_mutex);
	assert(_shutdown == false);

	kthread_t* self = kthread_self();
	assert(self != NULL);  // Yield cannot be called by a thread that was not
						   // created by the system.

	uthread_t* cur = self->running;
	transfer_elapsed_time(self, cur);

	//HEAPprint(_waiting_uthreads, uthread_print);

	// Yield this `kthread` to the highest-priority waiting `uthread` if it has
	// a higher priority than the currently-running `uthread`.
	if (waiting_uthread_has_priority_over(cur))
	{
		// Get another `uthread` from the heap to be run.
		uthread_t* next = NULL;
		HEAPextract(_waiting_uthreads, (void **) &next);
		self->running = next;

		// Save the current `uthread` to the heap.
		HEAPinsert(_waiting_uthreads, (void *) cur);

		// TODO: consider possibility of race conditions!
		pthread_mutex_unlock(&_mutex);
		kthread_handoff(cur, next);
	}
	else
	{
		pthread_mutex_unlock(&_mutex);
	}
}



void uthread_exit()
{
	pthread_mutex_lock(&_mutex);
	kthread_t* self = kthread_self();
	int self_tid;
	int self_tgid;
	pthread_mutex_unlock(&_mutex);

	// If the calling thread is not a `kthread` created by the system, block on a
	// mutex until there are no running `kthreads`.
	if (self == NULL) {
		pthread_mutex_lock(&_shutdown_mutex);
		_shutdown = true;
		pthread_mutex_unlock(&_shutdown_mutex);
		return;
	}

	pthread_mutex_lock(&_mutex);
	assert(_shutdown == false);

	self = kthread_self();
	uthread_t* prev = self->running;
	uthread_t* next = NULL;

	// TODO: print prev->running_time for debug.

	// Stop running the `prev` `uthread`, and clean up any references to it.
	self->running = NULL;
	free(prev);

	// Check if a `uthread` can use this kthread.
	if (HEAPsize(_waiting_uthreads) > 0)
	{
		// Use this `kthread` to run a different `uthread`.
		uthread_t* next = NULL;
		HEAPextract(_waiting_uthreads, (void **) &next);
		assert(next != NULL);
		self->running = next;

		kthread_update_timestamps(self);

		// TODO: consider possibility of race conditions.
		pthread_mutex_unlock(&_mutex);
		setcontext(&(next->ucontext));
	}
	else
	{
		// There are no `uthread`s that might use `self`. Kill `self`.
		// Find out more about `self` first.
		self_tid = self->tid;
		self_tgid = getpid();

		// Clean up `kthread`-associated system data structures.
		_num_kthreads--;

		// If this was the last `kthread`, then the system-shutdown mutex is unlocked.
		// Free lock and kill self.
		if (_num_kthreads == 0) {
			pthread_mutex_unlock(&_shutdown_mutex);
		}

		pthread_mutex_unlock(&_mutex);
		tgkill(self_tid, self_tgid, SIGKILL);
		assert(false);  // Control should never reach here.
	}
}



/* Define primary helper functions. **********************************************/

void kthread_handoff(uthread_t* prev, uthread_t* next)
{
	assert(prev != NULL);
	assert(next != NULL);

	swapcontext(&(prev->ucontext), &(next->ucontext));
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
}


/**
 * This function is expected to be a `start_routine` for `clone()`
 *
 * This interprets the given void pointer as a pointer to a `kthread_t`.
 */
int kthread_runner(void* ptr)
{
	kthread_t* kt = ptr;
	assert(kt != NULL);
	assert(kt->running != NULL);

	kt->tid = gettid();
	kthread_update_timestamps(kt);

	// Switch to running the `uthread`.
	setcontext(&(kt->running->ucontext));

	assert(false);  // Execution should never reach here.
}


/**
 * Returns a pointer to the `kthread_t` that is executing the function. If the
 * thread which is calling the function is not a `kthread` created by by this
 * system, then `NULL` is returned.
 */
kthread_t* kthread_self()
{
	kthread_t* cur = _kthreads;
	kthread_t* end = _kthreads + _max_num_kthreads;
	int self_tid = gettid();
	while (cur < end)
	{
		if (cur->tid == self_tid) {
			assert(cur->running != NULL);  // TODO: make this more robust
			return cur;
		}
		cur++;
	}
	return NULL;
}


/**
 * Run the given user thread on the given kernel thread. The kernel thread must
 * not already be active.
 */
int kthread_create(kthread_t* kt, uthread_t* ut)
{
	// TODO: everything!
	assert(kt->running == NULL);
	kt->running = ut;

	// Try using clone instead:
	void *child_stack;
	child_stack = (void *)malloc(CLONE_STACK_SIZE);
	child_stack += CLONE_STACK_SIZE - 1;
	int pid = clone(kthread_runner, child_stack, CLONE_VM|CLONE_FILES, kt);
	assert(pid > 0);
	return pid;
}


/**
 * Updates the time info in both the given `kthread` and the given `uthread`
 * by transfering the time which has elapsed since the timestamps of `kthread`
 * were updated to the running time of `uthread`.
 *
 * This means that the `kthread` will be updated with new timestamps and that
 * any time which has elapsed since the last timestamps will be added to the
 * running time of the given `uthread`. Both the elapsed `utime` and the elapsed
 * `stime` are added.
 *
 * Note that the given `kt` is assumed to be the the same as would be returned
 * by `kthread_self()`.
 */
void transfer_elapsed_time(kthread_t* kt, uthread_t* ut)
{
	assert(kt == kthread_self());

	struct timeval tv;
	struct timeval prev_utime_timestamp = kt->utime_timestamp;
	struct timeval prev_stime_timestamp = kt->stime_timestamp;

	kthread_update_timestamps(kt);

	// Add the change to `utime` to `running_time`.
	timersub(&(kt->utime_timestamp), &(prev_utime_timestamp), &tv);
	timeradd(&(ut->running_time), &tv, &(ut->running_time));

	// Add the change to `stime` to `running_time`.
	timersub(&(kt->stime_timestamp), &(prev_stime_timestamp), &tv);
	timeradd(&(ut->running_time), &tv, &(ut->running_time));
}




/* Define minor helper functions. ************************************************/

/**
 * Returns a pointer to a `kthread_t` slot which is which is not active. If
 * no such `kthread_t` slot exists, then `NULL` is returned.
 */
kthread_t* find_inactive_kthread()
{
	kthread_t* kthread = NULL;
	for (int idx = 0; idx < _max_num_kthreads; idx++) {
		if (_kthreads[idx].running == NULL) {
			kthread = _kthreads + idx;
			break;
		}
	}
	return kthread;
}



void get_thread_rusage(struct rusage* rv)
{
	const int RUSAGE_THREAD = 1;   // TODO: Fix this hack!
	getrusage(RUSAGE_THREAD, rv);  // Thread-specific rusage only available on linux.
}


void kthread_update_timestamps(kthread_t* kt)
{
	struct rusage ru;
	get_thread_rusage(&ru);
	kt->utime_timestamp = ru.ru_utime;
	kt->stime_timestamp = ru.ru_stime;
}


/**
 * Interprets `key1` and `key2` as pointers to `uthread_t` objects, and
 * compares them. The comparison is based on the running time of the two records.
 * In particular, given the two records, the record with the smaller running time will
 * have the greater priority.
 */
int uthread_priority(const void* key1, const void* key2)
{
	const struct timeval tv1 = ((const uthread_t*) key1)->running_time;
	const struct timeval tv2 = ((const uthread_t*) key2)->running_time;

	if (timercmp(&tv1, &tv2, <))
		return 1;
	if (timercmp(&tv1, &tv2, ==))
		return 0;
	if (timercmp(&tv1, &tv2, >))
		return -1;

	assert(false);  // All possibilities should be covered by the conditions above.
}


void uthread_print(const void* key) {
	const uthread_t* ut = key;
	const struct timeval running_time = ut->running_time;
	printf("uthread.running_time = %d.%06d\n", running_time.tv_sec, running_time.tv_usec);
}


/**
 * Returns true if and only if there is a `uthread` in `_waiting_uthreads` whose
 * priority is higher than the the priority of the given `uthread`.
 */
bool waiting_uthread_has_priority_over(uthread_t* ut)
{
	if (HEAPsize(_waiting_uthreads) > 0) {
		const uthread_t* highest_priority_waiting = HEAPpeek(_waiting_uthreads);
		return uthread_priority(ut, highest_priority_waiting) < 0;
	} else {
		return false;
	}
}
