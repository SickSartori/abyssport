/* <sys/time.h> for the runtime's POSIX code on Windows (posix.c). */
#ifndef ABYSS_POSIX_SYS_TIME_H
#define ABYSS_POSIX_SYS_TIME_H
#include <time.h>
#ifdef __cplusplus
extern "C" {
#endif
#ifndef _TIMEVAL_DEFINED   /* (winsock's, when a file has both) */
#define _TIMEVAL_DEFINED
struct timeval { long tv_sec, tv_usec; };
#endif
int gettimeofday(struct timeval *tv, void *zone);
#ifdef __cplusplus
}
#endif
#endif
