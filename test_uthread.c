#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>

#include "uthread.h"

int n_threads=1;
int myid=0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void do_something()
{
    int id,i,j;

    pthread_mutex_lock(&mutex);

    id=myid;
    myid++;
    printf("This is ult %d\n", id);

    if(n_threads<6){
        n_threads++;
        pthread_mutex_unlock(&mutex);
        uthread_create(do_something);
    } else {
	pthread_mutex_unlock(&mutex);
    }

    if(id%2==0)
        for(j=0;j<100;j++)
            for(i=0;i<1000000;i++) sqrt(i*119.89);

    printf("This is ult %d again\n", id);

    uthread_yield();

    printf("This is ult %d once more\n", id);

    if(id%2==0)
        for(j=0;j<100;j++)
            for(i=0;i<1000000;i++) sqrt(i*119.89);

    printf("This is ult %d exit\n", id);

    uthread_exit();
}

int main(int argc, char* argv[])
{
    int i;
    system_init(1);
    setbuf(stdout,NULL);
    int pid = uthread_create(do_something);
	uthread_exit();
	puts("Ending test program.\n");
}
