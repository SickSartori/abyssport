# Third-party software

abyssport's own code is free software under the GNU General Public License, version 2 or (at your option) any
later version (`LICENSE`). Code adapted from the projects below keeps a header that names its source, and each
library keeps its own licence, whose text travels with it.

## Adapted from

| Project | Licence | What abyssport takes |
|---|---|---|
| [Project Nativ](https://github.com/TheLostPantheon/nativ) at `4b1c428` | GPL 2.0 or later | Its runtime, file by file, each with a header naming the file it comes from: `src/core/` (kernel, memory, files, FIOS, PSARC, saved data, settings, the host), `src/gfx/` (the GPU boundary, the renderers, the shader translator), `src/audio/`, `src/input/`, `src/media/`, `src/ui/` (window, overlay), `src/debug/`, `src/platform/platform_win32.c`. `src/recomp/rt.h`, the recompiled code's header. `src/game/hooks.c`, its patches for Golden Abyss (`packs/PCSA00029/hooks.c`). `src/platform/posix/compat.h` and the file calls of `src/platform/posix.c` (from `runtime/platform/win32_compat.*`). The network part is not taken: `src/core/net.c` stands in for it. |
| [Vita3K](https://github.com/Vita3K/Vita3K) | GPL 2.0 or later | (nothing yet) |

Not in the repository, but used to make the game's code from the player's own copy: Project Nativ's recompiler,
decrypter and pipeline (Python), run from a checkout of it.

## In this repository

| Software | Version | Licence | Where |
|---|---|---|---|
| [Dear ImGui](https://github.com/ocornut/imgui) | 1.92.9b | MIT | `third_party/imgui/` (`LICENSE.txt`) |
| [stb_image and stb_image_write](https://github.com/nothings/stb) | 2.28 and 1.16 | MIT or public domain, your choice | `third_party/stb/` (the licence closes each header) |
| [LibAtrac9](https://github.com/Thealexbarney/LibAtrac9) | | MIT | `third_party/libatrac9/` (`LICENSE`) |
| [AMD FidelityFX SDK](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK): FSR 3.1's upscaler shaders | v1.1.4 | MIT | `third_party/fidelityfx/` (`LICENSE.txt`); compiled into `src/gfx/fsr_shaders.h` |
| HE-VAG coefficients from [vgmstream](https://github.com/vgmstream/vgmstream) | | ISC | `third_party/hevag/hevag_coefs.h` (in its header) |
| [Input Prompts](https://kenney.nl/assets/input-prompts) by Kenney: the touch card's button icons | 1.5A | CC0 1.0 | `third_party/kenney-prompts/` (`License.txt`) |

## Downloaded when the build is configured

Pinned by version and hash, linked statically.

| Software | Version | Licence | Use |
|---|---|---|---|
| [SDL](https://libsdl.org) | 3.4.20 | zlib | The window, the GPU API the renderer draws through, audio output, controllers |
| [zlib](https://zlib.net) | 1.3.1 | zlib | The game's archives (PSARC) are deflated |
