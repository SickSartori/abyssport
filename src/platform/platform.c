#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <stdlib.h>
#include "platform.h"

void plat_random(void *buf, size_t n) {
	if (BCryptGenRandom(0, buf, (ULONG)n, BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) abort();   /* (never fails on a working system) */
}
