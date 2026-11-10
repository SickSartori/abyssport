/* The runtime's small interface to the operating system, beyond the POSIX names of posix/ (platform.c). */
#ifndef ABYSS_PLATFORM_H
#define ABYSS_PLATFORM_H
#include <stddef.h>

void plat_random(void *buf, size_t n);   /* n unpredictable bytes */
#endif
