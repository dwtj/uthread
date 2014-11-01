#include <stdbool.h>
#include <sys/time.h>
#include <assert.h>

#include "tvhelp.h"

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
