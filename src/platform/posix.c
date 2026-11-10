/*
 * The POSIX calls of the headers in posix/, over Win32.
 *
 * The file calls (pread, pwrite, realpath, utimes, rename, the binary file mode) are adapted from Project Nativ
 * (https://github.com/TheLostPantheon/nativ), runtime/platform/win32_compat.c at 4b1c428: GNU General Public
 * License, version 2 or (at your option) any later version.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utime.h>
#include "posix/pthread.h"
#include "posix/unistd.h"
#include "posix/dirent.h"
#include "posix/sys/time.h"

_Static_assert(sizeof(void *) == sizeof(SRWLOCK) && sizeof(pthread_cond_t) == sizeof(CONDITION_VARIABLE) && sizeof(pthread_once_t) == sizeof(INIT_ONCE), "posix/pthread.h");

int pthread_mutexattr_init(pthread_mutexattr_t *a) { *a = 0; return 0; }
int pthread_mutexattr_settype(pthread_mutexattr_t *a, int type) { *a = type; return 0; }
int pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *attr) { memset(m, 0, sizeof *m); m->recursive = attr && *attr == PTHREAD_MUTEX_RECURSIVE; return 0; }
/* (owner is only ever compared with the caller's own id: no other thread's stale read of it can match) */
int pthread_mutex_lock(pthread_mutex_t *m) {
	if (m->recursive && m->owner == GetCurrentThreadId()) { m->depth++; return 0; }
	AcquireSRWLockExclusive((SRWLOCK *)&m->lock);
	if (m->recursive) { m->owner = GetCurrentThreadId(); m->depth = 1; }
	return 0;
}
int pthread_mutex_trylock(pthread_mutex_t *m) {
	if (m->recursive && m->owner == GetCurrentThreadId()) { m->depth++; return 0; }
	if (!TryAcquireSRWLockExclusive((SRWLOCK *)&m->lock)) return EBUSY;
	if (m->recursive) { m->owner = GetCurrentThreadId(); m->depth = 1; }
	return 0;
}
int pthread_mutex_unlock(pthread_mutex_t *m) {
	if (m->recursive) { if (--m->depth) return 0; m->owner = 0; }
	ReleaseSRWLockExclusive((SRWLOCK *)&m->lock);
	return 0;
}
static BOOL CALLBACK once_start(PINIT_ONCE once, PVOID fn, PVOID *ctx) { (void)once; (void)ctx; ((void (*)(void))fn)(); return TRUE; }
int pthread_once(pthread_once_t *once, void (*fn)(void)) { InitOnceExecuteOnce((INIT_ONCE *)once, once_start, (PVOID)fn, 0); return 0; }

int pthread_cond_init(pthread_cond_t *c, const void *attr) { (void)attr; InitializeConditionVariable((CONDITION_VARIABLE *)c); return 0; }
int pthread_cond_destroy(pthread_cond_t *c) { (void)c; return 0; }
int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m) { SleepConditionVariableSRW((CONDITION_VARIABLE *)c, (SRWLOCK *)&m->lock, INFINITE, 0); return 0; }
int pthread_cond_timedwait(pthread_cond_t *c, pthread_mutex_t *m, const struct timespec *until) {
	struct timespec now; clock_gettime(CLOCK_REALTIME, &now);
	long long ms = (until->tv_sec - now.tv_sec) * 1000LL + (until->tv_nsec - now.tv_nsec + 999999) / 1000000;   /* rounded up: never early */
	if (ms < 0) ms = 0;
	if (ms > 0x7fffffff) ms = 0x7fffffff;
	return SleepConditionVariableSRW((CONDITION_VARIABLE *)c, (SRWLOCK *)&m->lock, (DWORD)ms, 0) ? 0 : ETIMEDOUT;
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

int gettimeofday(struct timeval *tv, void *zone) {
	(void)zone;
	struct timespec t; clock_gettime(CLOCK_REALTIME, &t);
	tv->tv_sec = (long)t.tv_sec; tv->tv_usec = t.tv_nsec / 1000;
	return 0;
}

/* every open()/fopen() binary unless it asks otherwise: text mode turns CR LF into LF and ends a read at the first 0x1A */
__attribute__((constructor)) static void binary_files(void) { _set_fmode(_O_BINARY); }

/* positional I/O through the handle with an OVERLAPPED offset -- which on a synchronous handle moves the file
 * pointer too, so it is put back: the descriptor read()/write() position stays where POSIX leaves it */
static intptr_t rw_at(int fd, void *buf, size_t n, long long off, int write) {
	HANDLE h = (HANDLE)_get_osfhandle(fd);
	if (h == INVALID_HANDLE_VALUE) { errno = EBADF; return -1; }
	LARGE_INTEGER zero = {0}, cur; if (!SetFilePointerEx(h, zero, &cur, FILE_CURRENT)) { errno = EIO; return -1; }
	OVERLAPPED ov; memset(&ov, 0, sizeof ov); ov.Offset = (DWORD)((uint64_t)off & 0xffffffffu); ov.OffsetHigh = (DWORD)((uint64_t)off >> 32);
	DWORD done = 0; BOOL ok = write ? WriteFile(h, buf, (DWORD)n, &done, &ov) : ReadFile(h, buf, (DWORD)n, &done, &ov);
	DWORD err = ok ? 0 : GetLastError();
	SetFilePointerEx(h, cur, 0, FILE_BEGIN);
	if (!ok && err != ERROR_HANDLE_EOF) { errno = err == ERROR_ACCESS_DENIED ? EACCES : EIO; return -1; }
	return (intptr_t)done;
}
intptr_t pread(int fd, void *buf, size_t n, long long off) { return rw_at(fd, buf, n, off, 0); }
intptr_t pwrite(int fd, const void *buf, size_t n, long long off) { return rw_at(fd, (void *)buf, n, off, 1); }
int truncate(const char *path, long long size) {
	int fd = _open(path, _O_WRONLY | _O_BINARY);
	if (fd < 0) return -1;
	int e = _chsize_s(fd, size); _close(fd);
	if (e) { errno = e; return -1; }
	return 0;
}
int utimes(const char *path, const struct timeval tv[2]) {
	struct __utimbuf64 u = { tv[0].tv_sec, tv[1].tv_sec };
	return _utime64(path, &u);
}
char *realpath(const char *path, char *resolved) {
	char full[MAX_PATH];
	if (!_fullpath(full, path, sizeof full)) { errno = ENAMETOOLONG; return 0; }
	if (GetFileAttributesA(full) == INVALID_FILE_ATTRIBUTES) { errno = ENOENT; return 0; }
	for (char *p = full; *p; p++) if (*p == 92) *p = 47;   /* (backslashes to slashes) */
	size_t n = strlen(full); while (n > 3 && full[n - 1] == 47) full[--n] = 0;   /* (no trailing separator, as POSIX) */
	if (!resolved) return _strdup(full);
	memcpy(resolved, full, n + 1);
	return resolved;
}
int posix_rename(const char *from, const char *to) {
	if (MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) return 0;
	DWORD e = GetLastError();
	errno = e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND ? ENOENT : e == ERROR_ACCESS_DENIED || e == ERROR_SHARING_VIOLATION ? EACCES : e == ERROR_NOT_SAME_DEVICE ? EXDEV : EIO;
	return -1;
}

struct DIR { intptr_t h; int first; struct __finddata64_t f; struct dirent e; };
DIR *opendir(const char *path) {
	char pat[MAX_PATH + 3];
	if (snprintf(pat, sizeof pat, "%s/*", path) >= (int)sizeof pat) { errno = ENAMETOOLONG; return 0; }
	DIR *d = malloc(sizeof *d);
	if (!d) return 0;
	d->h = _findfirst64(pat, &d->f); d->first = 1;
	if (d->h == -1) { free(d); errno = ENOENT; return 0; }
	return d;
}
struct dirent *readdir(DIR *d) {
	if (!d->first && _findnext64(d->h, &d->f)) return 0;
	d->first = 0;
	snprintf(d->e.d_name, sizeof d->e.d_name, "%s", d->f.name);
	return &d->e;
}
int closedir(DIR *d) { _findclose(d->h); free(d); return 0; }
