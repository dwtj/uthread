#ifndef _UTHREAD_H
#define _UTHREAD_H

/**
 * A poorly named but necessary alias for `uthread_system_init()`.
 */
void system_init(int max_number_of_klt);

void uthread_system_init(int max_number_of_klt);

int uthread_create(void (*func)());

void uthread_yield();

void uthread_exit();

#endif  /* _UTHREAD_H */
