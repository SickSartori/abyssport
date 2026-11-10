# abyssport

A native PC port of Uncharted: Golden Abyss (PS Vita): the game's ARM code recompiled ahead of time into a
Windows program, with a runtime that stands in for the Vita's system and hardware.

Nothing of the game ships. You need your own copy of the game, US release PCSA00029 version 1.00, and the Vita
firmware update file (PUP).

Status: early. The game starts, a new campaign begins and the prologue plays; nothing has been played through yet.

Controls: [docs/controls.md](docs/controls.md).

## Building

For now abyssport is built from source, on Windows. You need Python 3.11 or later and CMake 3.25 or later; the
compiler is fetched for you.

1. `pip install -r tools/requirements.txt`
2. `python tools/get_toolchain.py`: llvm-mingw (Clang) and Ninja into `toolchain/`. Nothing is installed in the
   system.
3. `python tools/make_game.py <your copy of the game> <PSVUPDAT.PUP>`: the game's code and data, made from your
   copy and Sony's firmware update file (3.74) into `game/library`. About ten minutes.
4. `cmake --preset pc-x64`, then `cmake --build --preset pc-x64-release`. The first build takes about ten minutes:
   the game is 425 MB of C.
5. `build\pc-x64\abyssport.exe`, the launcher (or `play.cmd`, which starts the game directly)

## Credits

abyssport adapts code from two GPL projects, credited in each file that comes from them:

- [Project Nativ](https://github.com/TheLostPantheon/nativ): the recompiler, the runtime and the game's fixes.
- [Vita3K](https://github.com/Vita3K/Vita3K): the reference for the Vita's system modules, touch and motion.

Other third-party software: [THIRD_PARTY.md](THIRD_PARTY.md).

## Licence

GNU General Public License, version 2 or (at your option) any later version: [LICENSE](LICENSE).
