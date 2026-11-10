/*
 * Put in front of every source adapted from Project Nativ (the build's /FI): the POSIX file calls that Windows'
 * C runtime has under another name or shape, so the call sites stay as they are. 64-bit file offsets throughout.
 * What needs code is in posix.c. No <windows.h> here.
 *
 * Adapted from Project Nativ (https://github.com/TheLostPantheon/nativ), runtime/platform/win32_compat.h at
 * 4b1c428: GNU General Public License, version 2 or (at your option) any later version.
 */
#ifndef ABYSS_POSIX_COMPAT_H
#define ABYSS_POSIX_COMPAT_H
#include <direct.h>
#include <fcntl.h>
#include <io.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

typedef intptr_t ssize_t;
#define off_t long long
#define stat _stat64                       /* (the structure and the function) */
#define fstat _fstat64
#define lseek _lseeki64
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
#define F_OK 0
#define W_OK 2
#define R_OK 4
#ifndef PATH_MAX
#define PATH_MAX 1024
#endif
#define mkdir(path, mode) _mkdir(path)     /* (no permission bits on Windows) */
#define timegm _mkgmtime
#define fsync(fd) _commit(fd)
#define sync() ((void)0)                   /* (the Vita's flush-everything: nothing to do here) */
#define random() rand()
/* rename onto an existing file: POSIX replaces it (every "write a temporary, rename it over" save relies on that) */
#define rename(from, to) posix_rename((from), (to))
int posix_rename(const char *from, const char *to);
ssize_t pread(int fd, void *buf, size_t n, off_t off);      /* positional: the descriptor's own offset untouched */
ssize_t pwrite(int fd, const void *buf, size_t n, off_t off);
char *realpath(const char *path, char *resolved);           /* an existing path, absolute, '/'-separated; 0 if it doesn't exist */
int truncate(const char *path, off_t size);
#include <sys/time.h>                                       /* (posix/sys/time.h: struct timeval) */
int utimes(const char *path, const struct timeval tv[2]);   /* access and modification times, whole seconds */
#endif
