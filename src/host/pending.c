/*
 * What the kernel calls in parts of the runtime that are not here yet (the display, file IO, the wait statistics):
 * the least that keeps it right, each to be replaced by the real part.
 */
#include <pthread.h>
#include "hle.h"

/* the display's clock. shortcut: the real clock only; the fixed clock of scripted runs comes with the display */
uint64_t guest_time_us(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (uint64_t)t.tv_sec * 1000000 + t.tv_nsec / 1000; }
int guest_clock_fixed(void) { return 0; }
void guest_time_slept(uint32_t us) { (void)us; }
void display_vblank_callbacks(void) {}   /* shortcut: no display, so no vblank to notify callbacks of */

/* file IO's descriptors: a host descriptor as the game sees it (Nativ's io.c: + 0x100) */
uint32_t io_guest_fd(int fd) { return (uint32_t)fd + 0x100; }

/* the wait statistics (a diagnostic of Nativ's, runtime/debug/waitstats.c): off */
int waitstats_on(void) { return 0; }
uint64_t waitstats_now(void) { return 0; }
void waitstats_begin(uint64_t t0) { (void)t0; }
void waitstats_wait(uint64_t t0, uint64_t woken_at) { (void)t0; (void)woken_at; }
void waitstats_sleep(uint64_t t0) { (void)t0; }
void waitstats_name(const char *n) { (void)n; }
