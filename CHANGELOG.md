# Changelog

## Unreleased

### Added

- The game runs: its ARM code and Sony's libraries it loads (libc and FIOS from the game, the graphics library and
  its GPU services from the firmware update file) are recompiled to C from the player's own copy and built into the
  program. The intro film, the title, the menus, a new campaign and the prologue play, with sound; saved games are
  written and read back.
- The runtime, adapted from Project Nativ: the Vita's kernel, files, FIOS and PSARC archives, saved data, the
  software and GPU renderers (through SDL3) with the shader translator, audio (NGS, ATRAC9), films (H.264 through
  FFmpeg's DLLs), touch, motion and controllers, the window, the in-game menu, and the patches for Golden Abyss
  (field of view, wider aspect ratios, 60 fps, mouse look, view distance).
- The launcher (`abyssport.exe`), in the look of mc3port's and sm2port's: whether the game is ready; the Original,
  Enhanced and Custom styles; picture, effects, game and audio settings; the keyboard's keys and the mouse's five
  buttons, each on any action; and "How to play", a tutorial for keyboard and mouse, a DualSense or an Xbox
  controller (chosen at its top, first guessed from the controller connected; the controllers are Kenney's
  drawings, their buttons lighting up as the scene presses them): nine small moving scenes that show
  how the console's touch screen, rear pad, tilt, camera, aiming and the port's menu are played with that device. In Italian and English.
  Play starts the game and the launcher comes back when it ends.
- `tools/make_game.py`: the game's code and data made from your own copy and Sony's firmware update file (3.74), in
  one step. The decrypter, the analyser and the recompiler it runs are in `tools/`.
- The build: llvm-mingw and Ninja, fetched by `tools/get_toolchain.py` (nothing is installed), with the `pc-x64`
  preset. SDL3, zlib and FFmpeg are downloaded at configure time, pinned by version and hash.
- The shader recipes for the game (`res/shaders.txt`), so its shaders are built as it starts and not at their
  first use.
- The release (`tools/make_package.py`): the launcher, the runtime compiled, the tools, an embedded Python and the
  compiler, in one zip. The first time, the launcher asks for your copy of the game and the firmware file and
  makes the game on your PC (Prepare: decrypted, recompiled, compiled by `tools/build_game.py`), with its progress
  on screen; Cancel or closing the launcher stops it, and a second run picks up the files already compiled.
- abyssport's own icon and emblem, drawn from code by `tools/art/make_art.py`, and four backgrounds for the launcher
  before there is a copy of the game (a temple at dawn, a chasm in the jungle, a cave, ruins by moonlight), generated
  with Magnific. Nothing in them comes from the game.
- `abyss.log`: what the game prints, beside its files, for bug reports.
- Paths with accented letters work (UTF-8 as the programs' code page).
- `docs/controls.md`: the controls, key by key. F1 opens the in-game menu, F10 cycles the performance display.
- `play.cmd`: starts the game from the build tree without the launcher.
