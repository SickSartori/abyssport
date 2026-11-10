/*
 * abyssport: the host of the recompiled game, first stage. It reserves the Vita's 4 GiB address space, loads
 * the game's memory image and those of the libraries recompiled with it (Sony's libc and FIOS, from the game's
 * own sce_module), gives the main thread a stack and its thread-local storage, and calls module_start.
 * An import goes to the library that exports it. No system call is implemented yet: every other import is
 * logged by name with its arguments and returns 0, so a run shows which calls the game makes and how far it gets.
 *
 * usage: abyss <game.img> <module_start, hex> [imports to log before stopping, default 100]
 *
 * Adapted from Project Nativ (https://github.com/TheLostPantheon/nativ), runtime/core/main.c at 4b1c428 (the
 * image loader, the guest allocator, the thread-local block, the main thread's registers): GNU General Public
 * License, version 2 or (at your option) any later version.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "rt.h"

/* the recompiled code's own tables (game.c) */
extern const char *recomp_import_name(uint32_t addr);
extern GuestFn recomp_lookup(uint32_t addr);

uint8_t *recomp_mem;
__thread uint32_t recomp_excl_addr, recomp_excl_val;   /* rt.h's exclusive monitor */
__thread int recomp_excl_valid;
__thread uint64_t recomp_excl_val64;
__thread uint32_t recomp_tpidruro;                      /* the guest thread's TLS pointer */

static uint32_t tls_template[3];   /* the program's: address, size, block size */
static struct { uint32_t addr; char name[60]; } *exports;   /* every loaded library's exported functions */
static uint32_t nexports, module_starts[16];
static int nmodule_starts;

/* the libraries recompiled with the game: each one's generated code registers its tables here before main runs */
typedef struct { const char *name; GuestFn (*lookup)(uint32_t); const char *(*import_name)(uint32_t); } Module;
static Module modules[16];
static int nmodules;
void recomp_register_module(const char *name, GuestFn (*lookup)(uint32_t), const char *(*import_name)(uint32_t), int (*is_arm)(uint32_t)) {
	(void)is_arm;
	if (nmodules < 16) modules[nmodules++] = (Module){name, lookup, import_name};
}
static GuestFn guest_lookup(uint32_t a) {
	GuestFn f = recomp_lookup(a);
	for (int i = 0; !f && i < nmodules; i++) f = modules[i].lookup(a);
	return f;
}
static const char *import_name_any(uint32_t a) {
	const char *n = recomp_import_name(a);
	for (int i = 0; !n && i < nmodules; i++) n = modules[i].import_name(a);
	return n;
}
/* an import stub's implementation in a loaded library (0: none), remembered per stub.
 * shortcut: the cache has no lock; it needs one (or atomics) with the first guest thread */
static uint32_t module_target(uint32_t stub, const char *name) {
	static struct { uint32_t stub, target; } cache[4096];
	uint32_t h = (stub >> 1) & 4095, t = 0;
	if (cache[h].stub == stub) return cache[h].target;
	if (strcmp(name, "__tls_get_addr"))   /* (the host's: thread-local storage is per host thread) */
		for (uint32_t i = 0; i < nexports && !t; i++) if (!strcmp(exports[i].name, name)) t = exports[i].addr;
	cache[h].stub = stub; cache[h].target = t;
	return t;
}
static unsigned long long ncalls, budget = 100;

static void die(const char *what, const char *arg) { fprintf(stderr, "abyss: %s%s\n", what, arg); exit(1); }

/* everything below GUEST_HEAP is the image's; above it, a bump allocator
 * shortcut: one thread and nothing is freed; needs a lock and a real heap with the kernel's memory blocks */
#define GUEST_HEAP 0x10000000u
#define GUEST_HEAP_END 0x80000000u
static uint32_t guest_alloc(uint32_t size, uint32_t align) {
	static uint32_t top = GUEST_HEAP;
	uint32_t a = (top + align - 1) & ~(align - 1);
	if (a + size > GUEST_HEAP_END || a + size < a) die("guest heap exhausted", "");
	top = a + size;
	return a;
}

static void rd(void *p, size_t n, FILE *f, const char *path) { if (fread(p, 1, n, f) != n) die("truncated image: ", path); }

/* the image (the recompiler's mkimage): "VRIM", count, count x (address, size, has bytes, the bytes), then tagged
 * records: TLS, the program's thread-local template; EXPT, a library's exported functions; MSTA, its module_start.
 * The others (imported variables, process parameters) wait for the kernel */
static void load_image(const char *path, int program) {
	FILE *f = fopen(path, "rb");
	if (!f) die("can't open ", path);
	char tag[4]; uint32_t n, h[3];
	rd(tag, 4, f, path); rd(&n, 4, f, path);
	if (memcmp(tag, "VRIM", 4)) die("not a memory image: ", path);
	for (uint32_t i = 0; i < n; i++) {
		rd(h, 12, f, path);
		if ((uint64_t)h[0] + h[1] > 0xffffffffu) die("a block past 4 GiB in ", path);
		if (h[2]) rd(G(h[0]), h[1], f, path);
		fprintf(stderr, "image %#010x +%#x\n", h[0], h[1]);
	}
	while (fread(tag, 1, 4, f) == 4) {
		if (!memcmp(tag, "TLS", 4)) rd(program ? tls_template : h, 12, f, path);
		else if (!memcmp(tag, "PPRM", 4)) rd(&n, 4, f, path);
		else if (!memcmp(tag, "MSTA", 4)) { rd(&n, 4, f, path); if (nmodule_starts < 16) module_starts[nmodule_starts++] = n; }
		else if (!memcmp(tag, "VARS", 4)) { rd(&n, 4, f, path); fseek(f, (long)n * 64, SEEK_CUR); }
		else if (!memcmp(tag, "EXPT", 4)) {
			rd(&n, 4, f, path);
			if (n > 65536 || !(exports = realloc(exports, (nexports + n) * sizeof *exports))) die("bad export table in ", path);
			rd(exports + nexports, n * sizeof *exports, f, path);
			nexports += n;
		}
		else die("unknown record in ", path);
	}
	fclose(f);
}

static void stop(const char *why) {
	fprintf(stderr, "abyss: %s after %llu import calls\n", why, ncalls);
	exit(0);
}

/* a call out of the recompiled code: an import, or code reached through a pointer */
void recomp_call_external(Cpu *c, uint32_t addr) {
	const char *name = import_name_any(addr);
	GuestFn f = guest_lookup((name ? module_target(addr, name) : addr) & ~1u);
	if (f) { f(c); return; }
	if (!name) {
		fprintf(stderr, "call to %#x: neither recompiled code nor an import (lr %#x)\n", addr, c->r[14]);
		stop("stopped");
	}
	fprintf(stderr, "#%llu %s(%#x, %#x, %#x, %#x) lr %#x\n", ++ncalls, name, c->r[0], c->r[1], c->r[2], c->r[3], c->r[14]);
	if (ncalls >= budget) stop("stopped");
	c->r[0] = 0;
}

void recomp_unimplemented(Cpu *c, uint32_t addr) {
	fprintf(stderr, "unimplemented instruction at %#x (lr %#x)\n", addr, c->r[14]);
	stop("stopped");
}

/* a guest setjmp. shortcut: one buffer, enough until something calls longjmp; needs Nativ's table by guest buffer */
RecompJmp *recomp_setjmp_begin(Cpu *c) {
	static RecompJmp j;
	j.guest = c->r[0];
	return &j;
}

int main(int argc, char **argv) {
	if (argc < 3) { fprintf(stderr, "usage: %s <game.img> <module_start, hex> [imports to log, default 100]\n", argv[0]); return 1; }
	if (argc > 3) budget = strtoull(argv[3], 0, 0);
	/* shortcut: all 4 GiB committed at once (charged to the page file, touched pages only use memory); when that
	 * is refused, reserve and commit pages on their first touch, as Nativ's platform_win32.c does */
	recomp_mem = VirtualAlloc(0, 1ull << 32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!recomp_mem) die("can't get the 4 GiB of the Vita's address space (page file too small?)", "");
	load_image(argv[1], 1);
	for (int i = 0; i < nmodules; i++) {   /* each library's image, beside the program's: <name>.img */
		char path[1024]; const char *slash = strrchr(argv[1], '/'), *back = strrchr(argv[1], '\\');
		if (back > slash) slash = back;
		snprintf(path, sizeof path, "%.*s%s.img", slash ? (int)(slash + 1 - argv[1]) : 0, argv[1], modules[i].name);
		load_image(path, 0);
	}
	uint32_t entry = strtoul(argv[2], 0, 16);
	GuestFn start = recomp_lookup(entry & ~1u);
	if (!start) die("not a recompiled function: ", argv[2]);

	_mm_setcsr(_mm_getcsr() | 0x8040);   /* flush-to-zero, denormals-are-zero: the Vita runs user code in RunFast mode */
	uint32_t tls = tls_template[2] > 0x4000 ? tls_template[2] : 0x4000;
	recomp_tpidruro = guest_alloc(tls, 64);
	if (tls_template[1]) memcpy(G(recomp_tpidruro), G(tls_template[0]), tls_template[1]);

	static Cpu c;
	c.r[13] = guest_alloc(1u << 20, 4096) + (1u << 20) - 16;   /* main thread: 1 MiB of stack */
	c.r[14] = 0xfffffffe;
	c.fpscr = 0x03000000;                                       /* RunFast */
	for (int i = 0; i < nmodule_starts; i++) {   /* the libraries first, as the loader does */
		/* shortcut: on the main thread, below its stack; the console gives each a thread of its own (needs the kernel's threads) */
		Cpu m = c; m.r[13] -= 0x10000;
		GuestFn ms = guest_lookup(module_starts[i] & ~1u);
		if (!ms) die("a library's module_start is not recompiled code", "");
		fprintf(stderr, "library module_start %#x\n", module_starts[i]);
		ms(&m);
		fprintf(stderr, "library module_start returned %#x\n", m.r[0]);
	}
	fprintf(stderr, "module_start %#x\n", entry);
	start(&c);
	fprintf(stderr, "module_start returned %#x\n", c.r[0]);
	stop("module_start returned");
}
