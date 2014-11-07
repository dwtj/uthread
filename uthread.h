#ifndef _UTHREAD_H
#define _UTHREAD_H

/**
 * A poorly-named alias for `uthread_system_init()`. (See the documentation for
 * that function.)
 */
void system_init(int max_number_of_klt);


/**
 * Initializes the `uthread` system. This can only be called once during an
 * executable's lifetime. The system will only make a number of kthreads up to
 * the given maximum.
 */
void uthread_system_init(int max_num_kthreads);


/**
 * This function creates a `uthread` that will run the given `func` when it is
 * executed. The given `func()` MUST call `uthread_exit()` before it reaches
 * the end of `func()`.
 *
 * If the current number of running `kthread`s is not already at the maximum,
 * then the new `uthread` will be run immediately. Otherwise (i.e. if the new
 * `uthread` cannot be run yet), the `uthread` will be added to the `priority`
 * queue of waiting `uthread`s, where it will have top priority.
 */
int uthread_create(void (*func)());


/**
 * This is the key to cooperative threading in the system. It must only be
 * called by threads created with `uthread_create(). By calling this, a
 * `uthread` can handoff the `kthread` which it is running on to a waiting
 * `uthread` with higher priority.
 *
 * If there is no waiting `uthread` with a higher priority than the currently
 * running `uthread`, then the current `uthread` just continues executing.
 */
void uthread_yield();


/**
 * This function can be used in two ways.
 *
 * (1) If the calling thread is a `kthread` (i.e. a kernel thread which was
 * created via `uthread_create()`), then the `uthread` which is being run on
 * that `kthread` terminates.
 *
 * (2) If the calling thread is not a `kthread` (i.e. it is code running that
 * was not initiated by calling `uthread_create()`), then the thread will
 * block until all `uthread`s have exited. The main application thread is a
 * good example of such a thread.
 */
void uthread_exit();

#endif  /* _UTHREAD_H */
