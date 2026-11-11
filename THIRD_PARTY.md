# Third-party software

abyssport's own code is free software under the GNU General Public License, version 2 or (at your option) any
later version (`LICENSE`). Code adapted from the projects below keeps a header that names its source, and each
library keeps its own licence, whose text travels with it.

## Adapted from

| Project | Licence | What abyssport takes |
|---|---|---|
| [Project Nativ](https://github.com/TheLostPantheon/nativ) at `4b1c428` | GPL 2.0 or later | Its runtime, file by file, each with a header naming the file it comes from: `src/core/` (kernel, memory, files, FIOS, PSARC, saved data, settings, the host), `src/gfx/` (the GPU boundary, the renderers, the shader translator), `src/audio/`, `src/input/`, `src/media/`, `src/ui/` (window, overlay), `src/debug/`, `src/platform/` (its Windows layer). `src/recomp/rt.h`, the recompiled code's header. `src/game/hooks.c`, its patches for Golden Abyss (`packs/PCSA00029/hooks.c`). `res/shaders.txt`, its shader recipes for the game (`packs/PCSA00029/shaders.txt`: no game bytes in it). The network part is not taken: `src/core/net.c` stands in for it. |
| [Vita3K](https://github.com/Vita3K/Vita3K) | GPL 2.0 or later | (nothing yet) |
| [mc3port](https://github.com/SickSartori/mc3port) | MIT, (c) 2026 SickSartori | The launcher's look, layout and widgets (`src/launcher/launcher.cpp`, from its `src/pc/launcher_ui.cpp`), on SDL3 |

Also from Project Nativ, the tools that make the game's code from the player's own copy (Python, each file with a
line naming its source): `tools/recompiler/` (the analyser, the ARM to C recompiler, the memory images),
`tools/decrypt/` (the decrypter of games and firmware files) and `tools/PCSA00029.toml` (the game's facts and the
addresses its patches hook). `tools/make_game.py`, which runs them, is abyssport's.

The decrypter is itself ported from other projects, which Project Nativ credits:

| Part | Ported from | Licence |
|---|---|---|
| `tools/decrypt/vitadecrypt/` `keys.py`, `sce_self.py`, `pkg.py`, `pup.py`, `sfo.py` | [Vita3K](https://github.com/Vita3K/Vita3K)'s `packages/` | GPL 2.0 or later |
| the key values and SELF/PUP handling Vita3K credits | [TeamMolecule's sceutils](https://github.com/TeamMolecule/sceutils) | MIT |
| the PKG format Vita3K credits | [pkg2zip](https://github.com/mmozeiko/pkg2zip) | Unlicense |
| `tools/decrypt/vitadecrypt/pfs.py` | [psvpfstools](https://github.com/motoharu-gosuto/psvpfstools) and Vita3K's [psvpfsparser](https://github.com/Vita3K/psvpfsparser) | none published |
| `tools/decrypt/nidmap.tsv` (function names by NID) | [VitaSDK's NID database](https://github.com/vitasdk/vita-headers) | MIT |

## In this repository

| Software | Version | Licence | Where |
|---|---|---|---|
| [Dear ImGui](https://github.com/ocornut/imgui) | 1.92.9b | MIT | `third_party/imgui/` (`LICENSE.txt`) |
| [stb_image and stb_image_write](https://github.com/nothings/stb) | 2.28 and 1.16 | MIT or public domain, your choice | `third_party/stb/` (the licence closes each header) |
| [LibAtrac9](https://github.com/Thealexbarney/LibAtrac9) | | MIT | `third_party/libatrac9/` (`LICENSE`) |
| [AMD FidelityFX SDK](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK): FSR 3.1's upscaler shaders | v1.1.4 | MIT | `third_party/fidelityfx/` (`LICENSE.txt`); compiled into `src/gfx/fsr_shaders.h` |
| HE-VAG coefficients from [vgmstream](https://github.com/vgmstream/vgmstream) | | ISC | `third_party/hevag/hevag_coefs.h` (in its header) |
| [Barlow Semi Condensed](https://github.com/jpt/barlow) by Jeremy Tribby: the launcher's letters | | SIL Open Font License 1.1 | `third_party/fonts/` (`OFL.txt`) |
| [Input Prompts](https://kenney.nl/assets/input-prompts) by Kenney: the touch card's button icons | 1.5A | CC0 1.0 | `third_party/kenney-prompts/` (`License.txt`) |

## Downloaded when the build is configured

Pinned by version and hash, linked statically.

| Software | Version | Licence | Use |
|---|---|---|---|
| [SDL](https://libsdl.org) | 3.4.20 | zlib | The window, the GPU API the renderer draws through, audio output, controllers |
| [zlib](https://zlib.net) | 1.3.1 | zlib | The game's archives (PSARC) are deflated |

FFmpeg is not linked in: its libraries are used as they are built, as DLLs beside the program, with its licence
(`FFmpeg-LICENSE.txt`).

| Software | Version | Licence | Use |
|---|---|---|---|
| [FFmpeg](https://ffmpeg.org): libavcodec, libavutil and libswresample, from [BtbN's builds](https://github.com/BtbN/FFmpeg-Builds) (`win64-lgpl-shared`, 31 October 2026) | 8.1.3 | LGPL 2.1 or later | The H.264 decoder behind the game's films |

## Used to build, not part of the program

`tools/get_toolchain.py` downloads these into `toolchain/`, pinned by version and hash.

| Software | Version | Licence | Use |
|---|---|---|---|
| [llvm-mingw](https://github.com/mstorsjo/llvm-mingw) | 20261104 (Clang 23.1.5) | Apache 2.0 with LLVM exceptions; mingw-w64's runtime under its own permissive licences | The compiler; its C and C++ libraries and winpthreads are linked into the program |
| [Ninja](https://ninja-build.org) | 1.13.2 | Apache 2.0 | Runs the build |

## Artwork

The launcher's backgrounds (`res/launcher/bg_*.jpg`) were generated for abyssport with
[Magnific](https://www.magnific.ai) (Magnific One) from text alone, and resized; no image of the game was used. The
icon and the emblem are drawn from code by `tools/art/make_art.py`. The two controllers in "How to play"
(`res/launcher/pad_dualsense.png`, `pad_xbox.png`) are `controller_playstation5.png` and `controller_xboxseries.png`
from [Kenney](https://kenney.nl)'s Input Prompts 1.5A (CC0), the same pack as `third_party/kenney-prompts`.

## In the release

`tools/make_package.py` puts these in the release, pinned by version and hash, with their licences in `licenses/`:
the player's PC makes the game with them.

| Software | Version | Licence | Use |
|---|---|---|---|
| llvm-mingw, its x86-64 part (above) | 20261104 | as above | Compiles the game's code and links it to the runtime |
| [Python](https://www.python.org), the embeddable distribution | 3.14.7 | PSF License | Runs `tools/make_game.py` and `tools/build_game.py` |
| [Capstone](https://www.capstone-engine.org) | 5.0.9 | BSD 3-clause | The disassembler the recompiler reads the game's code with |
| [PyCryptodome](https://www.pycryptodome.org) | 3.24.0 | BSD 2-clause, public domain parts | The decrypter's AES and SHA |
