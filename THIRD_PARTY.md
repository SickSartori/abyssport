# Third-party software

abyssport's own code is free software under the GNU General Public License, version 2 or (at your option) any
later version (`LICENSE`). Code adapted from the projects below keeps its original copyright header, and each
library keeps its own licence, whose text travels with it.

## Adapted from

| Project | Licence | What abyssport takes |
|---|---|---|
| [Project Nativ](https://github.com/TheLostPantheon/nativ) | GPL 2.0 or later | `src/recomp/rt.h` (the recompiled code's header, unchanged); `src/host/main.c` (the image loader, the guest allocator, the import dispatch, the main thread's start, from `runtime/core/main.c`); `src/kernel/` (unchanged: `kernel.c`, `sysmem.c`, `libc.c`, `io.c`, `fios.c`, `psarc.c`, `apputil.c`, `settings.c` and their headers; `hle.c` reduced: threads, synchronisation, memory blocks, time, files, FIOS and PSARC archives, saved data, settings); `src/gfx/guestwatch.h`; `src/platform/posix/compat.h` and the file calls of `src/platform/posix.c` (from `runtime/platform/win32_compat.*`) |
| [Vita3K](https://github.com/Vita3K/Vita3K) | GPL 2.0 or later | (nothing yet) |

## In this repository

(nothing yet)

## Downloaded when the build is configured

| Software | Version | Licence | Use |
|---|---|---|---|
| [zlib](https://zlib.net) | 1.3.1 | zlib | The game's archives (PSARC) are deflated |
