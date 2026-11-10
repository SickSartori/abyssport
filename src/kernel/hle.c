/*
 * The system-call registry: every subsystem file exports one table (name -> implementation); hle_find searches
 * them by import name. The host resolves each import stub once and keeps the answer (src/host/main.c).
 *
 * Adapted from Project Nativ (https://github.com/TheLostPantheon/nativ), runtime/core/hle.c at 4b1c428:
 * GNU General Public License, version 2 or (at your option) any later version.
 */
#include <string.h>
#include "hle.h"

static const HleEntry *const tables[] = { hle_kernel, hle_libc, hle_io, hle_sysmem, hle_apputil, hle_fios };

HleFn hle_find(const char *name) {
	for (size_t t = 0; t < sizeof tables / sizeof *tables; t++)
		for (const HleEntry *e = tables[t]; e->name; e++)
			if (!strcmp(e->name, name)) return e->fn;
	return 0;
}

/* an imported variable's slot. shortcut: none known -- this game and its two libraries import no variable (the
 * host refuses an image that does); Nativ's table comes back with a library that needs it */
uint32_t hle_var_addr(const char *name) { (void)name; return 0; }
