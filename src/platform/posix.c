/* The POSIX calls of posix/pthread.h and posix/unistd.h, over Win32. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <errno.h>
#include <process.h>
#include <stdlib.h>
#include "posix/pthread.h"
#include "posix/unistd.h"

_Static_assert(sizeof(pthread_mutex_t) == sizeof(SRWLOCK) && sizeof(pthread_cond_t) == sizeof(CONDITION_VARIABLE), "posix/pthread.h");

int pthread_mutex_init(pthread_mutex_t *m, const void *attr) { (void)attr; InitializeSRWLock((SRWLOCK *)m); return 0; }
int pthread_mutex_lock(pthread_mutex_t *m) { AcquireSRWLockExclusive((SRWLOCK *)m); return 0; }
int pthread_mutex_trylock(pthread_mutex_t *m) { return TryAcquireSRWLockExclusive((SRWLOCK *)m) ? 0 : EBUSY; }
int pthread_mutex_unlock(pthread_mutex_t *m) { ReleaseSRWLockExclusive((SRWLOCK *)m); return 0; }

int pthread_cond_init(pthread_cond_t *c, const void *attr) { (void)attr; InitializeConditionVariable((CONDITION_VARIABLE *)c); return 0; }
int pthread_cond_destroy(pthread_cond_t *c) { (void)c; return 0; }
int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m) { SleepConditionVariableSRW((CONDITION_VARIABLE *)c, (SRWLOCK *)m, INFINITE, 0); return 0; }
int pthread_cond_timedwait(pthread_cond_t *c, pthread_mutex_t *m, const struct timespec *until) {
	struct timespec now; clock_gettime(CLOCK_REALTIME, &now);
	long long ms = (until->tv_sec - now.tv_sec) * 1000LL + (until->tv_nsec - now.tv_nsec + 999999) / 1000000;   /* rounded up: never early */
	if (ms < 0) ms = 0;
	if (ms > 0x7fffffff) ms = 0x7fffffff;
	return SleepConditionVariableSRW((CONDITION_VARIABLE *)c, (SRWLOCK *)m, (DWORD)ms, 0) ? 0 : ETIMEDOUT;
}
int pthread_cond_signal(pthread_cond_t *c) { WakeConditionVariable((CONDITION_VARIABLE *)c); return 0; }
int pthread_cond_broadcast(pthread_cond_t *c) { WakeAllConditionVariable((CONDITION_VARIABLE *)c); return 0; }

int pthread_attr_init(pthread_attr_t *a) { a->stack = 0; a->detached = 0; return 0; }
int pthread_attr_setstacksize(pthread_attr_t *a, size_t bytes) { a->stack = bytes; return 0; }
int pthread_attr_setdetachstate(pthread_attr_t *a, int state) { a->detached = state; return 0; }
typedef struct { void *(*fn)(void *); void *arg; } Start;
static unsigned __stdcall thread_start(void *p) { Start s = *(Start *)p; free(p); s.fn(s.arg); return 0; }
int pthread_create(pthread_t *t, const pthread_attr_t *a, void *(*fn)(void *), void *arg) {
	Start *s = malloc(sizeof *s);
	if (!s) return ENOMEM;
	s->fn = fn; s->arg = arg;
	/* (the stack size is a reservation: committed as it grows) */
	HANDLE h = (HANDLE)_beginthreadex(0, a ? (unsigned)a->stack : 0, thread_start, s, STACK_SIZE_PARAM_IS_A_RESERVATION, 0);
	if (!h) { free(s); return EAGAIN; }
	if (a && a->detached) CloseHandle(h);
	*t = h;   /* (a detached thread's handle is closed: for nothing but pthread_detach's no-op below) */
	return 0;
}
int pthread_detach(pthread_t t) { CloseHandle(t); return 0; }
int pthread_join(pthread_t t, void **result) { WaitForSingleObject(t, INFINITE); CloseHandle(t); if (result) *result = 0; return 0; }
pthread_t pthread_self(void) { return GetCurrentThread(); }
int pthread_setname_np(pthread_t t, const char *name) {
	wchar_t w[64];
	if (!MultiByteToWideChar(CP_UTF8, 0, name, -1, w, 64)) return EINVAL;
	return SUCCEEDED(SetThreadDescription(t, w)) ? 0 : EINVAL;
}
_Noreturn void pthread_exit(void *result) { (void)result; _endthreadex(0); abort(); }

int clock_gettime(int clock, struct timespec *t) {
	if (clock == CLOCK_MONOTONIC) {
		static LARGE_INTEGER f; LARGE_INTEGER n;
		if (!f.QuadPart) QueryPerformanceFrequency(&f);
		QueryPerformanceCounter(&n);
		t->tv_sec = n.QuadPart / f.QuadPart; t->tv_nsec = (long)((n.QuadPart % f.QuadPart) * 1000000000LL / f.QuadPart);
	} else {
		FILETIME ft; GetSystemTimePreciseAsFileTime(&ft);
		unsigned long long n = ((unsigned long long)ft.dwHighDateTime << 32 | ft.dwLowDateTime) - 116444736000000000ULL;   /* 100 ns since 1970 */
		t->tv_sec = n / 10000000; t->tv_nsec = (long)(n % 10000000) * 100;
	}
	return 0;
}
/* a high-resolution waitable timer, one per thread: Sleep's steps are 1 to 16 ms */
int nanosleep(const struct timespec *t, struct timespec *left) {
	static __declspec(thread) HANDLE timer;
	if (!timer) timer = CreateWaitableTimerExW(0, 0, 2 /* CREATE_WAITABLE_TIMER_HIGH_RESOLUTION */, TIMER_ALL_ACCESS);
	LARGE_INTEGER due; due.QuadPart = -(t->tv_sec * 10000000LL + t->tv_nsec / 100);
	if (timer && SetWaitableTimer(timer, &due, 0, 0, 0, 0)) WaitForSingleObject(timer, INFINITE);
	else Sleep((DWORD)(t->tv_sec * 1000 + t->tv_nsec / 1000000));
	if (left) left->tv_sec = left->tv_nsec = 0;
	return 0;
}
int usleep(useconds_t us) { struct timespec t = {us / 1000000, (long)(us % 1000000) * 1000}; return nanosleep(&t, 0); }
struct tm *localtime_r(const time_t *t, struct tm *out) { return localtime_s(out, t) ? 0 : out; }
struct tm *gmtime_r(const time_t *t, struct tm *out) { return gmtime_s(out, t) ? 0 : out; }
