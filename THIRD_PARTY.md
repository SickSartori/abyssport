# Third-party licences

abyssport is free software under the GNU General Public License, version 2 or (at your option) any later version
(`LICENSE`). It contains code adapted from [Project Nativ](https://github.com/TheLostPantheon/nativ) and
[Vita3K](https://github.com/Vita3K/Vita3K), both GPL 2.0 or later; each file taken from them keeps a header naming its
source.

The software below is under licences that ask for its notice to travel with it. Each notice is in the folder named,
and the release carries them in `licenses/`.

| Software | Licence | Notice |
|---|---|---|
| [Dear ImGui](https://github.com/ocornut/imgui) 1.92.9b | MIT | `third_party/imgui/LICENSE.txt` |
| [LibAtrac9](https://github.com/Thealexbarney/LibAtrac9) | MIT | `third_party/libatrac9/LICENSE` |
| [AMD FidelityFX SDK](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK) v1.1.4 (FSR 3.1) | MIT | `third_party/fidelityfx/LICENSE.txt` |
| HE-VAG coefficients from [vgmstream](https://github.com/vgmstream/vgmstream) | ISC | the header of `third_party/hevag/hevag_coefs.h` |
| [Barlow Semi Condensed](https://github.com/jpt/barlow), the launcher's font | SIL Open Font License 1.1 | `third_party/fonts/OFL.txt` |
| [TeamMolecule's sceutils](https://github.com/TeamMolecule/sceutils), in `tools/decrypt/vitadecrypt/` | MIT | `tools/decrypt/licenses/sceutils-LICENSE.txt` |
| [VitaSDK's NID database](https://github.com/vitasdk/vita-headers), `tools/decrypt/nidmap.tsv` | MIT | `tools/decrypt/licenses/vita-headers-LICENSE.txt` |
| [FFmpeg](https://ffmpeg.org) 8.1.3 (libavcodec, libavutil, libswresample; [BtbN's](https://github.com/BtbN/FFmpeg-Builds) `win64-lgpl-shared` build), DLLs beside the game | LGPL 2.1 or later | `FFmpeg-LICENSE.txt` beside them; source: ffmpeg.org, tag `n8.1.3` |
| [llvm-mingw](https://github.com/mstorsjo/llvm-mingw) 20261104, the compiler; its mingw-w64 runtime and winpthreads are linked into the programs | Apache 2.0 with LLVM exceptions; mingw-w64's own licences | `toolchain/llvm-mingw/LICENSE.TXT`, `x86_64-w64-mingw32/share/mingw32/COPYING*` |
| [Python](https://www.python.org) 3.14.7, embedded in the release | PSF License | its `LICENSE.txt` |
| [Capstone](https://www.capstone-engine.org) 5.0.9, in the release's Python | BSD 3-clause | its `dist-info` |
| [PyCryptodome](https://www.pycryptodome.org) 3.24.0, in the release's Python | BSD 2-clause | its `dist-info` |
