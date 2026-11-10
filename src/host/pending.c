/*
 * What the kernel calls in parts of the runtime that are not here yet (the display, the GPU backend, the wait statistics):
 * the least that keeps it right, each to be replaced by the real part.
 */
#include <pthread.h>
#include "hle.h"

/* the display's clock. shortcut: the real clock only; the fixed clock of scripted runs comes with the display */
uint64_t guest_time_us(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (uint64_t)t.tv_sec * 1000000 + t.tv_nsec / 1000; }
int guest_clock_fixed(void) { return 0; }
void guest_time_slept(uint32_t us) { (void)us; }
void display_vblank_callbacks(void) {}   /* shortcut: no display, so no vblank to notify callbacks of */

/* the wait statistics (a diagnostic of Nativ's, runtime/debug/waitstats.c): off */
int waitstats_on(void) { return 0; }
uint64_t waitstats_now(void) { return 0; }
void waitstats_begin(uint64_t t0) { (void)t0; }
void waitstats_wait(uint64_t t0, uint64_t woken_at) { (void)t0; (void)woken_at; }
void waitstats_sleep(uint64_t t0) { (void)t0; }
void waitstats_name(const char *n) { (void)n; }

/* the GPU backend's write tracking of guest memory (src/gfx/guestwatch.h): no backend, so nothing is protected
 * and a host write into guest memory needs no bracket */
void gwatch_write_begin(uint32_t addr, uint32_t len) { (void)addr; (void)len; }
void gwatch_write_end(uint32_t addr, uint32_t len) { (void)addr; (void)len; }
void gwatch_read_open(uint32_t addr, uint32_t len) { (void)addr; (void)len; }
void gfx_gpu_builds_stop(void) {}   /* (the shader workers, stopped before exit: none yet) */

/* the overlay (overlay.h): no window yet, so no notice is shown and a save dialog answers by itself (apputil.c) */
void overlay_toast_titled(const char *title, const char *body, double seconds) { (void)title; (void)body; (void)seconds; }
int overlay_draws_hud(void) { return 0; }

/* libgxm's sceGxmShaderPatcherRegisterProgram, hooked when the library is recompiled: Nativ's renderer notes each
 * program the game registers there (its shader recipes). Nothing to note without a renderer */
void recomp_hook_gxm_register(Cpu *c) { (void)c; }
