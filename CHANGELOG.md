# Changelog

## 0.1.2 (2026-11-26)

### Added

- **An installer**, `abyssport-<version>-windows-x64-setup.exe`, beside the zip: it installs for your user without
  administrator rights, with shortcuts and an uninstaller, and offers to start the launcher, which asks for your copy
  of the game and the firmware file and makes the game as before. Uninstalling leaves the game it made and your saved
  games in the installation folder. The installer and its uninstaller are code signed, like the launcher.
- **Logs, as mc3port's**: `abyss.log` now goes to a `logs` folder beside the game's files, and the run before stays
  as `abyss-previous.log` (0.1.1 overwrote it at every start). The launcher's About page opens the folder. A crash
  writes `abyss-crash.dmp` there; when the launcher opens or the game comes back, the launcher puts the log and the
  dump in `abyss-crash-report.zip` and offers **Report the crash** (on About, and in place of Report a problem over
  the bar): a new GitHub issue with the version and the crash's first lines filled in, and the folder opened with the
  zip selected. Report a problem fills in the version too. RecompLauncher's log folder is the new one. Report the
  crash stands out: an amber border and a ring that widens and fades every second and a half.

## 0.1.1 (2026-11-25)

### Fixed

- The launcher (`abyssport.exe`) has version information (Properties, Details: abyssport launcher, its version) and
  is stripped of the debug sections the static C and C++ libraries brought: 0.1.0's was flagged on VirusTotal by
  Bitdefender's engine (Gen:Variant.Lazy, a heuristic false alarm), which held its listing on RecompLauncher. The
  game, `abyss.exe`, gets the same version information from the runtime.
- The launcher is code signed (Windows shows the publisher "Open Source Developer Gabriel Sartori"); README: Code
  signing policy. `tools/make_package.py --sign <zip>` signs the launcher of the zip the workflow built.
- The game started with its window minimised (a shortcut set so) no longer stops at once: the window is shown first.
  Minimised, it had no size, the game could not draw in it and stopped as its menu was made.

### Added

- README: two pictures of the game at a higher internal resolution, beside the ones at the Vita's own.

## 0.1.0 (2026-11-22)

### Added

- The release is built on GitHub (`.github/workflows/release.yml`): every push builds the package, and a tag `vX.Y.Z`
  makes a draft release with its SHA256SUMS. The launcher and the runtime build without a copy of the game
  (`abyss.exe` and its test are then left out).
- README: Known issues.
- F11 switches between the window and full screen during the game, as the launcher's Window mode row says; only
  the launcher's own window did.
- The launcher: Report a problem (a new issue on GitHub) and Support the project (Ko-fi) above its bottom bar, a
  Support the project row in About, and the hints' keys drawn as keyboard keys.
- The European copy of the game, PCSF00012 version 1.00, plays as the US one does (the same code; its
  `gamedata.bin` has other languages). The launcher's Settings, Game has the game's language, from the copy's own:
  English, French and Spanish in the US copy; English (UK), French, German, Italian and Dutch in the European one.
  Automatic follows Windows' language, else the copy's English.
- The launcher's first start says where the firmware file comes from, and Get it from Sony opens Sony's page for the PS
  Vita system software (update 3.74) in the launcher's language; the README links it too. The file is Sony's: the
  release doesn't carry it. Change copy, on the game's row of the Play page, makes the game again from another copy
  (the US one or the European), over the one already made; the row names the copy's own release.
- The launcher tells you when a newer abyssport is out (GitHub's latest release, asked when it opens): a notice over
  Play with what is new and the release page, and a row in About. Settings, Game, Updates switches it off.
- The game's own menus with a controller or the keyboard: the D-pad or the arrows move to the next button that way (a
  short line under it shows where), Cross / A or Space presses it, Circle / B, Esc or Backspace takes the menu's way
  back; on the main and pause menus' strips left and right turn to the next entry. The console's menus are touch only:
  these are the same touches, made for you. With no menu on screen the keys and buttons play the game as before.
- RecompLauncher: `recomplauncher.json` in the release (Play starts the game once it is prepared; resolution, frame
  rate, window, controls, mouse sensitivity and volume on the game's page; Easy Setup: the player's copy and the
  firmware file are asked for in RecompLauncher, and `tools/prepare.py` makes the game without a window, as Prepare
  does), and the release zip is named
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
- The in-game menu (F1, a controller's PS / Xbox button, or Select and Start together) is the launcher's own pages over
  the paused game: Resume, Settings, Controls, Back to the launcher, and Quit the game, which closes the launcher too.
  The internal resolution, full screen and the performance display apply at once, the keys when the menu closes; Esc
  goes back a step at a time. The game's cards and notes (quitting, the controller layout, shader preparation, the
  touch card, the saved data list and its questions) have the launcher's look and language, and grow with the window.
  The quit card (closing the window) offers Keep playing, Back to the launcher and Quit the game.
- On a keyboard and mouse the game's button prompts show the key or mouse button that plays each action, as set on
  the Controls page (Space, Shift, the mouse's right button...); a controller brings the console's symbols back. When
  the game waits for a touch, a note says how to give it with the device in use.
- Cover, roll and drop are on Left Shift, as in Uncharted on PC.
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
  first use. The file keeps LF line ends (`.gitattributes`), and the runtime reads it with Windows line ends too:
  checked out with them, every recipe had been refused.
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
