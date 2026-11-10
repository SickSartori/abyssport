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
- The rest of the runtime: the renderers (software and GPU, through SDL3), the shader translator, audio (NGS, ATRAC9),
  touch, motion and controllers, the window and the in-game overlay, and Project Nativ's patches for the game.
  The game draws its title screen. `play.cmd` starts it from the build tree.
- A new campaign starts and the prologue plays: the in-engine cutscene, then the first tutorial.
- The shader recipes for the game (`res/shaders.txt`), so its shaders are built as it starts.
- `tools/make_game.py`: the game's code and data made from your own copy and the firmware update file, in one step.
- `docs/controls.md`: how the console's sticks, touch screen, rear pad and gyroscope are played with a keyboard, a
  mouse or a controller.
- The game's films play: H.264 through FFmpeg's libavcodec, whose DLLs sit beside the program.
- The mouse's buttons can be put on any action, from the Controls page of the in-game menu.
- The tools that make the game's code (decrypter, analyser, recompiler) are in `tools/`: nothing outside the
  repository is needed any more.

### Changed

- The settings that outlive a copy of the game are in abyssport's own folder (`%APPDATA%byssport`), no longer in
  Project Nativ's.
- The build uses llvm-mingw and Ninja instead of Visual Studio's clang-cl: one compiler for the runtime and the
  game's code, the one Project Nativ builds with, fetched by `tools/get_toolchain.py`. Visual Studio is no longer
  needed.
- The launcher (`abyssport.exe`), in the look of mc3port's and sm2port's: whether the game is ready, the Original,
  Enhanced and Custom styles, picture, effects, game and audio settings, the keyboard's keys and the mouse's buttons,
  and a page on how the console's touch screen, rear pad, tilt and camera are played on a PC. In Italian and English.
  Play starts the game and the launcher comes back when it ends.
- The launcher's "How to play" page shows each of the console's inputs as a small moving scene: the keys, mouse or
  controller on one side, the console reacting on the other.
