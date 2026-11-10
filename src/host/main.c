/*
 * abyssport: the host of the recompiled game. It reserves the Vita's 4 GiB address space, loads the game's
 * memory image and those of the libraries recompiled with it (Sony's libc and FIOS, from the game's own
 * sce_module), runs each library's module_start on a thread of its own, as the loader does, then the game's on
 * the main thread.
 * An import goes to the library that exports it, else to the host's system call of that name (src/kernel).
 * One with neither is logged by name with its arguments, once, and returns 0: a run shows what is still missing.
 *
 * usage: abyss <game.img> <module_start, hex> [unimplemented imports to log before stopping, default 100]
 *
 * Adapted from Project Nativ (https://github.com/TheLostPantheon/nativ), runtime/core/main.c at 4b1c428 (the
 * image loader, the guest allocator, the modules' tables, the import dispatch, the thread-local block, the main
 * thread's registers): GNU General Public License, version 2 or (at your option) any later version.
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "hle.h"
#include "settings.h"

/* the recompiled code's own tables (game.c) */
extern const char *recomp_import_name(uint32_t addr);
extern GuestFn recomp_lookup(uint32_t addr);
uint32_t kernel_run_thread(const char *name, uint32_t entry);   /* kernel.c */

uint8_t *recomp_mem;
__thread uint32_t recomp_excl_addr, recomp_excl_val;   /* rt.h's exclusive monitor */
__thread int recomp_excl_valid;
__thread uint64_t recomp_excl_val64;
__thread uint32_t recomp_tpidruro;                      /* the guest thread's TLS pointer */
extern uint32_t process_param;                          /* the program's process parameters (sysmem.c: sceKernelGetProcessParam) */

static uint32_t tls_template[3];   /* the program's: address, size, block size */
static struct { uint32_t addr; char name[60]; } *exports;   /* every loaded library's exported functions */
static uint32_t nexports, module_starts[16];
static int nmodule_starts;
static pthread_mutex_t host_lock = PTHREAD_MUTEX_INITIALIZER;   /* the allocator, the import table and the log */
static unsigned long long nmissing, budget = 100;

static void die(const char *what, const char *arg) { fprintf(stderr, "abyss: %s%s\n", what, arg); exit(1); }

/* the Vita runs user code in RunFast mode (flush-to-zero, default NaN): the same on each host thread that runs
 * guest code (the control register is per thread) */
void recomp_runfast(void) { _mm_setcsr(_mm_getcsr() | 0x8040); }

/* everything below GUEST_HEAP is the image's; above it, a bump allocator hands out memory blocks, thread stacks
 * and host-made objects (nothing is given back: the kernel's own allocators sit on top) */
#define GUEST_HEAP 0x10000000u
#define GUEST_HEAP_END 0x80000000u
uint32_t guest_alloc(uint32_t size, uint32_t align) {
	static uint32_t top = GUEST_HEAP;
	if (align < 16) align = 16;
	pthread_mutex_lock(&host_lock);
	uint32_t a = (top + align - 1) & ~(align - 1);
	if (a + size > GUEST_HEAP_END || a + size < a) die("guest heap exhausted", "");
	top = a + size;
	pthread_mutex_unlock(&host_lock);
	return a;
}

/* the libraries recompiled with the game: each one's generated code registers its tables here before main runs */
typedef struct { const char *name; GuestFn (*lookup)(uint32_t); const char *(*import_name)(uint32_t); } Module;
static Module modules[16];
static int nmodules;
void recomp_register_module(const char *name, GuestFn (*lookup)(uint32_t), const char *(*import_name)(uint32_t), int (*is_arm)(uint32_t)) {
	(void)is_arm;
	if (nmodules < 16) modules[nmodules++] = (Module){name, lookup, import_name};
}
GuestFn guest_lookup(uint32_t a) {
	GuestFn f = recomp_lookup(a);
	for (int i = 0; !f && i < nmodules; i++) f = modules[i].lookup(a);
	return f;
}
static const char *import_name_any(uint32_t a) {
	const char *n = recomp_import_name(a);
	for (int i = 0; !n && i < nmodules; i++) n = modules[i].import_name(a);
	return n;
}

/* call guest code from a system call: a copy of the caller's registers, the stack below its own */
uint32_t guest_call(Cpu *parent, uint32_t fn, uint32_t a0, uint32_t a1) {
	GuestFn f = guest_lookup(fn & ~1u);
	if (!f) { fprintf(stderr, "guest callback %#x is not recompiled code\n", fn); return 0; }
	Cpu n = *parent;
	n.r[0] = a0; n.r[1] = a1;
	n.r[13] = (parent->r[13] - 512) & ~7u;
	n.r[14] = 0xfffffffe;
	f(&n);
	return n.r[0];
}

/* each guest thread's thread-local storage block, what TPIDRURO points at */
uint32_t tls_block(void) {
	uint32_t size = tls_template[2] > 0x4000 ? tls_template[2] : 0x4000;
	uint32_t a = guest_alloc(size, 64);
	memset(G(a), 0, size);
	if (tls_template[1]) memcpy(G(a), G(tls_template[0]), tls_template[1]);
	return a;
}

static void rd(void *p, size_t n, FILE *f, const char *path) { if (fread(p, 1, n, f) != n) die("truncated image: ", path); }

/* the image (the recompiler's mkimage): "VRIM", count, count x (address, size, has bytes, the bytes), then tagged
 * records: TLS, the program's thread-local template; PPRM, its process parameters; EXPT, a library's exported
 * functions; MSTA, its module_start. VARS (imported variables): none in this game or its two libraries */
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
		else if (!memcmp(tag, "PPRM", 4)) rd(&process_param, 4, f, path);
		else if (!memcmp(tag, "MSTA", 4)) { rd(&n, 4, f, path); if (nmodule_starts < 16) module_starts[nmodule_starts++] = n; }
		else if (!memcmp(tag, "VARS", 4)) { rd(&n, 4, f, path); if (n) die("imported variables, which the host doesn't fill yet, in ", path); }
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
	fprintf(stderr, "abyss: %s, %llu calls to unimplemented imports\n", why, nmissing);
	exit(0);
}

/* the imports the host answers even when a loaded library exports them: the plain memory and string functions,
 * faster here with the same result, and __tls_get_addr -- the host owns the threads and their thread-local blocks */
static int host_preferred(const char *n) {
	static const char *const fast[] = {"memcpy", "memmove", "memset", "memcmp", "memchr", "strlen", "strnlen", "strcmp",
		"strncmp", "strcpy", "strncpy", "strcat", "strncat", "strchr", "strrchr", "strstr", "sceClibMemcpy", "sceClibMemset",
		"sceClibMemmove", "sceClibMemcmp", "sceClibStrcmp", "sceClibStrncmp", "sceClibStrnlen", "__tls_get_addr", 0};
	for (int i = 0; fast[i]; i++) if (!strcmp(fast[i], n)) return 1;
	return 0;
}

/* an import's implementation, worked out once per stub: the library that exports it (as on the console), else
 * the host's system call */
typedef struct { uint32_t stub; GuestFn target; HleFn fn; int said; } Import;
static Import *resolve(uint32_t stub, const char *name) {   /* (host_lock held) */
	static Import imports[4096];
	Import *e = &imports[(stub >> 1) & 4095];
	if (e->stub == stub) return e;
	HleFn fn = hle_find(name);
	uint32_t t = 0;
	if (!(fn && host_preferred(name)))
		for (uint32_t i = 0; i < nexports && !t; i++) if (!strcmp(exports[i].name, name)) t = exports[i].addr;
	*e = (Import){stub, t ? guest_lookup(t & ~1u) : 0, fn, 0};
	return e;
}

/* a call out of the recompiled code: an import, or code reached through a pointer */
void recomp_call_external(Cpu *c, uint32_t addr) {
	const char *name = import_name_any(addr);
	if (!name) {
		GuestFn f = guest_lookup(addr & ~1u);
		if (f) { f(c); return; }
		fprintf(stderr, "call to %#x: neither recompiled code nor an import (lr %#x)\n", addr, c->r[14]);
		stop("stopped");
	}
	pthread_mutex_lock(&host_lock);
	Import *e = resolve(addr, name);
	GuestFn target = e->target; HleFn fn = e->fn;
	if (!target && !fn) {
		if (!e->said) { e->said = 1; fprintf(stderr, "unimplemented %s(%#x, %#x, %#x, %#x) lr %#x\n", name, c->r[0], c->r[1], c->r[2], c->r[3], c->r[14]); }
		if (++nmissing >= budget) stop("stopped");
	}
	pthread_mutex_unlock(&host_lock);
	if (target) target(c);
	else if (fn) fn(c);
	else c->r[0] = 0;
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
	if (argc < 3) { fprintf(stderr, "usage: %s <game.img> <module_start, hex> [unimplemented imports to log, default 100]\n", argv[0]); return 1; }
	if (argc > 3) budget = strtoull(argv[3], 0, 0);
	/* shortcut: all 4 GiB committed at once (charged to the page file, touched pages only use memory); when that
	 * is refused, reserve and commit pages on their first touch, as Nativ's platform_win32.c does */
	recomp_mem = VirtualAlloc(0, 1ull << 32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!recomp_mem) die("can't get the 4 GiB of the Vita's address space (page file too small?)", "");
	const char *slash = strrchr(argv[1], '/'), *back = strrchr(argv[1], 92);
	if (back > slash) slash = back;
	int dir = slash ? (int)(slash + 1 - argv[1]) : 0;   /* the image's folder, with its separator */
	if (!getenv("RECOMP_FS")) {   /* the game's drives (io.c): <folder>/fs/app0 is the game, beside its image */
		char fs[1024]; snprintf(fs, sizeof fs, "%.*sfs", dir, argv[1]);
		_putenv_s("RECOMP_FS", fs);
	}
	settings_load(getenv("RECOMP_SETTINGS"));   /* settings.ini in the working directory (settings.h) */
	load_image(argv[1], 1);
	for (int i = 0; i < nmodules; i++) {   /* each library's image, beside the program's: <name>.img */
		char path[1024]; snprintf(path, sizeof path, "%.*s%s.img", dir, argv[1], modules[i].name);
		load_image(path, 0);
	}
	uint32_t entry = strtoul(argv[2], 0, 16);
	GuestFn start = recomp_lookup(entry & ~1u);
	if (!start) die("not a recompiled function: ", argv[2]);

	recomp_runfast();
	recomp_tpidruro = tls_block();
	kernel_sched_init();
	static Cpu c;
	c.r[13] = guest_alloc(1u << 20, 4096) + (1u << 20) - 16;   /* main thread: 1 MiB of stack */
	c.r[14] = 0xfffffffe;
	c.fpscr = 0x03000000;                                       /* RunFast */
	for (int i = 0; i < nmodule_starts; i++) {   /* the libraries first, each on a thread of its own, as the loader does */
		fprintf(stderr, "library module_start %#x\n", module_starts[i]);
		fprintf(stderr, "library module_start returned %#x\n", kernel_run_thread("module_start", module_starts[i]));
	}
	fprintf(stderr, "module_start %#x\n", entry);
	start(&c);
	fprintf(stderr, "module_start returned %#x\n", c.r[0]);
	stop("module_start returned");
}
