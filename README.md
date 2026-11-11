# abyssport

A native PC port of Uncharted: Golden Abyss (PS Vita): the game's ARM code recompiled ahead of time into a
Windows program, with a runtime that stands in for the Vita's system and hardware.

Nothing of the game ships. You need your own copy of the game, US release PCSA00029 version 1.00, and the Vita
firmware update file (PUP).

Status: early. The game starts, a new campaign begins and the prologue plays; nothing has been played through yet.

Controls: [docs/controls.md](docs/controls.md).

## Playing

1. Unzip the release anywhere with about 9 GB free (Windows 10 version 1903 or later, 64-bit).
2. Open `abyssport.exe`. The first time it asks for your copy of the game (the `PCSA00029` folder copied off the
   console, with its licence `sce_sys/package/work.bin`) and the firmware file `PSVUPDAT.PUP` (system update 3.74).
3. Press **Prepare**. Once only, about 15 minutes: the game is decrypted, its code recompiled to C and compiled on
   your PC with the compiler that comes with the release. Nothing is installed in the system and nothing is
   downloaded.
4. Press **Play**. In the game, F1 opens the menu with every setting and control.

The release carries its own source code in `source/` (GPL), and the licences of what it ships in `licenses/`.

What the game prints goes to `abyss.log` beside its files (`game\library\decrypted\PCSA00029\recomp`): attach it to a
bug report.

## Building

abyssport is built on Windows. You need Python 3.11 or later and CMake 3.25 or later; the compiler is fetched for you.

1. `pip install -r tools/requirements.txt`
2. `python tools/get_toolchain.py`: llvm-mingw (Clang) and Ninja into `toolchain/`. Nothing is installed in the
   system.
3. `python tools/make_game.py <your copy of the game> <PSVUPDAT.PUP>`: the game's code and data, made from your
   copy and Sony's firmware update file (3.74) into `game/library`. About ten minutes.
4. `cmake --preset pc-x64`, then `cmake --build --preset pc-x64-release`. The first build takes about ten minutes:
   the game is 425 MB of C.
5. `build\pc-x64\abyssport.exe`, the launcher (or `play.cmd`, which starts the game directly)

The release: `python tools/make_package.py` puts the launcher, the runtime compiled (`libabyssrt.a`), the tools,
an embedded Python and the x86-64 part of llvm-mingw in `dist/abyssport-<version>` and zips it. On the player's PC
`tools/build_game.py` compiles the game's code and links it to that runtime.

## Supporting the project

abyssport is free and always will be: no paid builds, no early access, no locked features. If you enjoy it and want
to say thanks, you can support its author on [GitHub Sponsors](https://github.com/sponsors/SickSartori) or
[Ko-fi](https://ko-fi.com/sicksartori). It is never needed to play, and it buys nothing: it gives more time for fixes,
features and the next port.

## Licence

GNU General Public License, version 2 or (at your option) any later version: [LICENSE](LICENSE). The notices of the
third-party code it contains: [THIRD_PARTY.md](THIRD_PARTY.md).
