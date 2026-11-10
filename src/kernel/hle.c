/*
 * The system-call registry: every subsystem file exports one table (name -> implementation); hle_find searches
 * them by import name. The host resolves each import stub once and keeps the answer (src/host/main.c).
 *
 * Adapted from Project Nativ (https://github.com/TheLostPantheon/nativ), runtime/core/hle.c at 4b1c428:
 * GNU General Public License, version 2 or (at your option) any later version.
 */
#include <stdio.h>
#include <string.h>
#include "hle.h"

static const HleEntry *const tables[] = { hle_kernel, hle_libc, hle_io, hle_sysmem, hle_apputil, hle_fios };

HleFn hle_find(const char *name) {
	for (size_t t = 0; t < sizeof tables / sizeof *tables; t++)
		for (const HleEntry *e = tables[t]; e->name; e++)
			if (!strcmp(e->name, name)) return e->fn;
	return 0;
}

/* imported variables arrive one by one (the host, from the image); some refer to each other (a C++ vtable and
 * its type_info), so they are filled once all are known */
static struct { char name[60]; uint32_t addr; } vars[256];
static int nvars;
void hle_import_var(const char *name, uint32_t addr) {
	if (nvars < 256) { strncpy(vars[nvars].name, name, 59); vars[nvars++].addr = addr; }
}
uint32_t hle_var_addr(const char *name) {
	for (int i = 0; i < nvars; i++) if (!strcmp(vars[i].name, name)) return vars[i].addr;
	return 0;
}
void hle_import_vars_done(void) {
	for (int i = 0; i < nvars; i++) {
		int ok = libc_import_var(vars[i].name, vars[i].addr) || cxx_import_var(vars[i].name, vars[i].addr);
		fprintf(stderr, "import variable %s at %#x%s\n", vars[i].name, vars[i].addr, ok ? "" : " (left zero)");
	}
}
