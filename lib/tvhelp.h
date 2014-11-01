#ifndef _TVHELP_H
#define _TVHELP_H

struct timeval timeval_add(struct timeval fst, struct timeval snd);
int timeval_cmp(struct timeval fst, struct timeval snd);
bool timeval_is_leq(struct timeval fst, struct timeval snd);
struct timeval timeval_diff(struct timeval fst, struct timeval snd);

#endif  /* _TVHELP_H */
