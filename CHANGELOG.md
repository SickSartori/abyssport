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
