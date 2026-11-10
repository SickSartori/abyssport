# Changelog

## Unreleased

### Added

- The project: licence, credits and layout.
- The build: the game's recompiled code compiles into a library, with clang-cl (`pc-x64` preset).
- The program starts the game's code: `abyss` loads the game's memory image, calls its entry point and logs each
  system call the game makes (none is implemented yet).
- Sony's libc and FIOS, the copies the game ships, recompiled and loaded with the game: its calls to them run their
  own code.
- The Vita's kernel: threads, mutexes, semaphores, events, memory blocks and time. The game allocates its heaps,
  starts its worker threads and reaches the set-up of its graphics.
- Files, Sony's FIOS over them, PSARC archives, saved data and settings: the game opens its data (`gamedata.bin`).
  The only calls left without an implementation at start-up are the graphics library's.
- Sony's graphics library (GXM) and its GPU services, recompiled from the player's own firmware update file (3.74)
  and loaded with the game: its graphics calls run Sony's code. The game goes on to set up its audio and input.
