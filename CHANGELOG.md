# Changelog

## Unreleased

### Added

- The launcher: Report a problem (a new issue on GitHub) and Support the project (Ko-fi) above its bottom bar, a
  Support the project row in About, and the hints' keys drawn as keyboard keys.
- The European copy of the game, PCSF00012 version 1.00, plays as the US one does (the same code; its
  `gamedata.bin` has other languages). The launcher's Settings, Game has the game's language, from the copy's own:
  English, French and Spanish in the US copy; English (UK), French, German, Italian and Dutch in the European one.
  Automatic follows Windows' language, else the copy's English.
- RecompLauncher: `recomplauncher.json` in the release (Play starts the game once it is prepared; resolution, frame
  rate, window, controls, mouse sensitivity and volume on the game's page), and the release zip is named
  `abyssport-<version>-windows-x64.zip`. The launcher's first start keeps a resolution or frame rate set from outside
  instead of applying the Enhanced style over it. With no controller connected, RecompLauncher's {pad}
  (`RECOMPLAUNCHER_PAD`) chooses How to play's device and the in-game menu's button names (PlayStation's, or the
  Xbox's).

- The game runs: its ARM code and Sony's libraries it loads (libc and FIOS from the game, the graphics library and
  its GPU services from the firmware update file) are recompiled to C from the player's own copy and built into the
  program. The intro film, the title, the menus, a new campaign and the prologue play, with sound; saved games are
  written and read back.
- The runtime: the Vita's kernel, files, FIOS and PSARC archives, saved data, the
  software and GPU renderers (through SDL3) with the shader translator, audio (NGS, ATRAC9), films (H.264 through
  FFmpeg's DLLs), touch, motion and controllers, the window, the in-game menu, and the patches for Golden Abyss
  (field of view, wider aspect ratios, 60 fps, mouse look, view distance).
- The launcher (`abyssport.exe`): whether the game is ready; the Original,
  Enhanced and Custom styles; picture, effects, game and audio settings; the keyboard's keys and the mouse's five
  buttons, each on any action; and "How to play", a tutorial for keyboard and mouse, a DualSense or an Xbox
  controller (chosen at its top, first guessed from the controller connected; one standard controller with each
  one's button symbols, lighting up as the scene presses them; the keys and buttons shown are the ones set on
  the other tabs): nine small moving scenes that show
  how the console's touch screen, rear pad, tilt, camera, aiming and the port's menu are played with that device. In
  Italian, English, Portuguese (Brazil) and Spanish, chosen from Windows' language.
  Play starts the game and the launcher comes back when it ends; the in-game menu's "Back to the launcher" does the
  same, for the settings that need a restart. Each page sits in a column from the top of the window to the bottom
  bar, with Play at its foot, and scrolls when it is longer than the window; a slider's value sits beside its grab,
  and the internal resolution is a drop-down list.
  A torch's warm light follows the mouse over the background (and wanders by itself when the mouse rests), lighting
  the dust in the air.
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
  before there is a copy of the game, or when the copy has no picture of its own (a temple at dawn, a chasm in the
  jungle, a cave, ruins by moonlight). Nothing in them comes from the game.
- `abyss.log`: what the game prints, beside its files, for bug reports.
- Paths with accented letters work (UTF-8 as the programs' code page).
- `docs/controls.md`: the controls, key by key. F1 opens the in-game menu, F10 cycles the performance display.
- `play.cmd`: starts the game from the build tree without the launcher.
