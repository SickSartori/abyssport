/*
 * The POSIX threads, clocks and sleeps the runtime's code calls, over Win32 (posix.c): the code adapted from
 * Project Nativ is written for POSIX, and this keeps its call sites as they are. Only what is used.
 * No <windows.h> here: its macros (min, max, ERROR, near...) would reach every file.
 */
#ifndef ABYSS_POSIX_PTHREAD_H
#define ABYSS_POSIX_PTHREAD_H
#include <stddef.h>
#include <stdint.h>
#include <time.h>

/* a mutex is an SRWLOCK and a condition a CONDITION_VARIABLE: one pointer each, zero when new. Not recursive. */
typedef struct { void *p; } pthread_mutex_t;
typedef struct { void *p; } pthread_cond_t;
#define PTHREAD_MUTEX_INITIALIZER {0}
#define PTHREAD_COND_INITIALIZER {0}
int pthread_mutex_init(pthread_mutex_t *m, const void *attr);
int pthread_mutex_lock(pthread_mutex_t *m);
int pthread_mutex_trylock(pthread_mutex_t *m);   /* 0 when taken */
int pthread_mutex_unlock(pthread_mutex_t *m);
int pthread_cond_init(pthread_cond_t *c, const void *attr);
int pthread_cond_destroy(pthread_cond_t *c);
int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m);
int pthread_cond_timedwait(pthread_cond_t *c, pthread_mutex_t *m, const struct timespec *until);   /* CLOCK_REALTIME; ETIMEDOUT */
int pthread_cond_signal(pthread_cond_t *c);
int pthread_cond_broadcast(pthread_cond_t *c);

typedef void *pthread_t;   /* the thread's handle */
typedef struct { size_t stack; int detached; } pthread_attr_t;
#define PTHREAD_CREATE_DETACHED 1
int pthread_attr_init(pthread_attr_t *a);
int pthread_attr_setstacksize(pthread_attr_t *a, size_t bytes);
int pthread_attr_setdetachstate(pthread_attr_t *a, int state);
int pthread_create(pthread_t *t, const pthread_attr_t *a, void *(*fn)(void *), void *arg);
int pthread_detach(pthread_t t);
int pthread_join(pthread_t t, void **result);    /* (the result is not kept: 0) */
pthread_t pthread_self(void);                    /* for pthread_setname_np only: not comparable */
int pthread_setname_np(pthread_t t, const char *name);
_Noreturn void pthread_exit(void *result);

#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
int clock_gettime(int clock, struct timespec *t);
int nanosleep(const struct timespec *t, struct timespec *left);   /* never interrupted: 0 */
struct tm *localtime_r(const time_t *t, struct tm *out);
struct tm *gmtime_r(const time_t *t, struct tm *out);
#endif
