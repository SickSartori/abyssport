#!/usr/bin/env python3
"""Put the release together in dist/abyssport-<version> and zip it. Local only: nothing is uploaded.

  python tools/make_package.py [--build build/pc-x64]

Needs a finished build (the launcher and the runtime library) and toolchain/llvm-mingw (tools/get_toolchain.py).
Downloads, pinned by version and SHA-256 and kept in dist/downloads: Python's embeddable distribution and the two
packages tools/make_game.py needs. No game code or data goes in: the player's launcher makes those from their copy.

  abyssport.exe, fonts/, art/, shaders.txt   the launcher and what it reads
  runtime/lib, runtime/include           the runtime compiled, and the two headers the game's code includes
  runtime/bin                            FFmpeg's DLLs, copied beside the abyss.exe the player's PC links
  tools/                                 make_game.py, build_game.py, prepare.py (RecompLauncher's setup) and what they run
  python/                                Python, embedded, with capstone and pycryptodome
  toolchain/llvm-mingw                   the compiler, its x86-64 part only
  licenses/, docs/                       the licences of what is shipped; the controls guide
  source/                                abyssport's source at HEAD (git archive): the GPL's offer
"""
import argparse
import fnmatch
import glob
import hashlib
import os
import re
import shutil
import subprocess
import sys
import urllib.request
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PYTHON = ("https://www.python.org/ftp/python/3.14.7/python-3.14.7-embed-amd64.zip",
          "d297e5ff019966817ad8502465176139f2d3d840fa4ed84b13bed399a6ab1f15")
WHEELS = (("https://files.pythonhosted.org/packages/50/e6/6f06fdb6a9ed32b2f7cd9c036b92d5324112c3ef7080f2c71efc367d40dd/capstone-5.0.9-py3-none-win_amd64.whl",
           "732cedbbb56d42e723f14d7af6387f1454194a820b4b96b56d1e53f865ef85d0"),
          ("https://files.pythonhosted.org/packages/e6/b4/4cd7b5b7f3e4c7012cbffc2295af8982b11c9f649079d90a90525200b5c4/pycryptodome-3.24.0-cp37-abi3-win_amd64.whl",
           "89a9c14b18f43491d7bec4440c7179eb51a56891c3070e41ae726cb3734c6b9b"))
# llvm-mingw without what an x86-64 build never runs: the other targets, the debugger, clangd, its own Python
TOOLCHAIN_DROP = ["aarch64-w64-mingw32", "armv7-w64-mingw32", "i686-w64-mingw32", "python", "busybox", "share", "libear", "libscanbuild",
                  "aarch64-*", "armv7-*", "arm64ec-*", "i686-*", "*uwp*", "*lldb*", "clangd*", "clang-tidy*", "libpython*", "*widl*",
                  "llvm-pdbutil*", "*-aarch64.*", "*-armhf.*", "*-i386.*", "*-arm64ec.*", "*.pdb"]
RUNTIME_LIBS = ["libabyssrt.a", "libatrac9.a", "libimgui.a", "_deps/zlib-build/libzlibstatic.a", "_deps/sdl3-build/libSDL3.a",
                "_deps/ffmpeg-src/lib/libavcodec.dll.a", "_deps/ffmpeg-src/lib/libavutil.dll.a"]


def die(msg):
    sys.exit("make_package: " + msg)


def fetch(url, sha, cache):
    path = os.path.join(cache, url.rsplit("/", 1)[1])
    if not os.path.isfile(path):
        print("downloading", url, flush=True)
        with urllib.request.urlopen(url) as r, open(path + ".part", "wb") as f: shutil.copyfileobj(r, f)
        os.replace(path + ".part", path)
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""): h.update(b)
    if h.hexdigest() != sha:
        os.remove(path); die(f"{url}: SHA-256 {h.hexdigest()}, not the pinned {sha}")
    return path


def unzip(path, dest):
    with zipfile.ZipFile(path) as z:
        for m in z.namelist():   # (a name in the zip never leaves dest)
            if os.path.commonpath([os.path.realpath(os.path.join(dest, m)), os.path.realpath(dest)]) != os.path.realpath(dest): die(f"unsafe path in {path}: {m!r}")
        z.extractall(dest)


def copytree(src, dst, drop=()):
    shutil.copytree(src, dst, ignore=lambda d, names: [n for n in names if n == "__pycache__" or any(fnmatch.fnmatch(n, p) for p in drop)])


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--build", default=os.path.join(ROOT, "build", "pc-x64"))
    a = ap.parse_args()
    build = os.path.abspath(a.build)
    version = re.search(r"project\(abyssport VERSION ([0-9.]+)", open(os.path.join(ROOT, "CMakeLists.txt"), encoding="utf-8").read()).group(1)
    toolchain = os.path.join(ROOT, "toolchain", "llvm-mingw")
    for f in [os.path.join(build, "abyssport.exe"), os.path.join(toolchain, "bin", "x86_64-w64-mingw32-clang.exe")] + [os.path.join(build, l) for l in RUNTIME_LIBS]:
        if not os.path.isfile(f): die(f"{f} is missing: build first (cmake --build --preset pc-x64-release) and fetch the toolchain")
    dist = os.path.join(ROOT, "dist"); cache = os.path.join(dist, "downloads"); os.makedirs(cache, exist_ok=True)
    out = os.path.join(dist, f"abyssport-{version}")
    if os.path.exists(out): shutil.rmtree(out)
    os.makedirs(out)

    print("the launcher", flush=True)
    shutil.copyfile(os.path.join(build, "abyssport.exe"), os.path.join(out, "abyssport.exe"))
    copytree(os.path.join(ROOT, "third_party", "fonts"), os.path.join(out, "fonts"))
    copytree(os.path.join(ROOT, "res", "launcher"), os.path.join(out, "art"))
    shutil.copyfile(os.path.join(ROOT, "res", "shaders.txt"), os.path.join(out, "shaders.txt"))
    # RecompLauncher reads it beside the exe; its setup step (prepare) builds the game with this release's runtime, so its
    # version is the release's: players who set up with an older one compile again
    with open(os.path.join(ROOT, "recomplauncher.json"), encoding="utf-8") as f: manifest = f.read()
    manifest, n = re.subn(r'("version":\s*")[^"]*(",?\s*\n\s*\},?\s*\n\s*"start")', lambda m: m.group(1) + version + m.group(2), manifest, count=1)
    if n != 1: die("recomplauncher.json: no prepare.version just before start")
    with open(os.path.join(out, "recomplauncher.json"), "w", encoding="utf-8") as f: f.write(manifest)

    print("the runtime", flush=True)
    for d in ("lib", "include", "bin"): os.makedirs(os.path.join(out, "runtime", d))
    for l in RUNTIME_LIBS: shutil.copyfile(os.path.join(build, l), os.path.join(out, "runtime", "lib", os.path.basename(l)))
    shutil.copyfile(os.path.join(ROOT, "src", "recomp", "rt.h"), os.path.join(out, "runtime", "include", "rt.h"))
    shutil.copyfile(os.path.join(ROOT, "src", "platform", "win32_compat.h"), os.path.join(out, "runtime", "include", "win32_compat.h"))
    for dll in glob.glob(os.path.join(glob.escape(build), "_deps", "ffmpeg-src", "bin", "*.dll")):
        if re.match(r"(avcodec|avutil|swresample)-\d+\.dll$", os.path.basename(dll)): shutil.copyfile(dll, os.path.join(out, "runtime", "bin", os.path.basename(dll)))

    print("the tools", flush=True)
    os.makedirs(os.path.join(out, "tools"))
    for f in ("make_game.py", "build_game.py", "prepare.py", "PCSA00029.toml"): shutil.copyfile(os.path.join(ROOT, "tools", f), os.path.join(out, "tools", f))
    for d in ("decrypt", "recompiler"): copytree(os.path.join(ROOT, "tools", d), os.path.join(out, "tools", d))

    print("Python", flush=True)
    py = os.path.join(out, "python")
    unzip(fetch(*PYTHON, cache), py)
    for url, sha in WHEELS: unzip(fetch(url, sha, cache), os.path.join(py, "Lib", "site-packages"))
    # the embedded Python reads its search path from this file only (PYTHONPATH is ignored): the packages and the tools
    pth = glob.glob(os.path.join(glob.escape(py), "python*._pth"))[0]
    zipname = os.path.basename(pth)[:-5] + ".zip"
    with open(pth, "w", newline="\r\n") as f: f.write(f"{zipname}\n.\nLib\\site-packages\n..\\tools\\decrypt\n..\\tools\\recompiler\nimport site\n")

    print("the compiler", flush=True)
    copytree(toolchain, os.path.join(out, "toolchain", "llvm-mingw"), TOOLCHAIN_DROP)

    print("licences and documents", flush=True)
    lic = os.path.join(out, "licenses"); os.makedirs(lic)
    # only the notices the licences ask for (THIRD_PARTY.md): zlib, SDL, stb and CC0 ask for none
    mingw = os.path.join(toolchain, "x86_64-w64-mingw32", "share", "mingw32")
    for src, name in ((os.path.join(ROOT, "LICENSE"), "abyssport-GPL-2.0.txt"), (os.path.join(build, "_deps", "ffmpeg-src", "LICENSE.txt"), "FFmpeg.txt"),
                      (os.path.join(toolchain, "LICENSE.TXT"), "llvm-mingw.txt"), (os.path.join(mingw, "COPYING.MinGW-w64-runtime.txt"), "mingw-w64-runtime.txt"),
                      (os.path.join(mingw, "COPYING.winpthreads.txt"), "winpthreads.txt"), (os.path.join(py, "LICENSE.txt"), "Python.txt"),
                      (os.path.join(ROOT, "third_party", "fonts", "OFL.txt"), "Barlow-OFL.txt"), (os.path.join(ROOT, "third_party", "imgui", "LICENSE.txt"), "DearImGui.txt"),
                      (os.path.join(ROOT, "third_party", "libatrac9", "LICENSE"), "LibAtrac9.txt"), (os.path.join(ROOT, "third_party", "fidelityfx", "LICENSE.txt"), "FidelityFX.txt"),
                      (os.path.join(ROOT, "tools", "decrypt", "licenses", "sceutils-LICENSE.txt"), "sceutils.txt"),
                      (os.path.join(ROOT, "tools", "decrypt", "licenses", "vita-headers-LICENSE.txt"), "vita-headers.txt")):
        if os.path.isfile(src): shutil.copyfile(src, os.path.join(lic, name))
        else: die(f"a licence the release must carry is missing: {src}")
    # vgmstream's ISC notice is the comment that opens the HE-VAG table
    hevag = open(os.path.join(ROOT, "third_party", "hevag", "hevag_coefs.h"), encoding="utf-8").read()
    with open(os.path.join(lic, "vgmstream-ISC.txt"), "w", encoding="utf-8", newline="\n") as f: f.write(hevag[:hevag.index("*/") + 2] + "\n")
    for whl in ("capstone", "pycryptodome"):
        for f in glob.glob(os.path.join(glob.escape(py), "Lib", "site-packages", whl + "-*.dist-info", "*LICEN*")): shutil.copyfile(f, os.path.join(lic, f"{whl}-{os.path.basename(f)}.txt"))
    for f in ("README.md", "THIRD_PARTY.md", "CHANGELOG.md"): shutil.copyfile(os.path.join(ROOT, f), os.path.join(out, f))   # (THIRD_PARTY.md: the licence notices, required)
    copytree(os.path.join(ROOT, "docs"), os.path.join(out, "docs"))

    # the GPL's source offer, kept simple: this version's source travels with its programs
    print("the source", flush=True)
    if subprocess.run(["git", "-C", ROOT, "status", "--porcelain", "--untracked-files=no"], capture_output=True, text=True).stdout.strip():
        print("  warning: uncommitted changes: the source archive is HEAD's, not what was built")
    os.makedirs(os.path.join(out, "source"))
    if subprocess.run(["git", "-C", ROOT, "archive", "--format=zip", f"--prefix=abyssport-{version}/", "-o",
                       os.path.join(out, "source", f"abyssport-{version}-source.zip"), "HEAD"]).returncode: die("git archive failed")

    print("zipping", flush=True)
    archive = shutil.make_archive(out + "-windows-x64", "zip", dist, os.path.basename(out))   # system and CPU in the name: launchers pick the file by them
    size = sum(os.path.getsize(os.path.join(d, f)) for d, _, fs in os.walk(out) for f in fs)
    print(f"done: {out} ({size >> 20} MB), {archive} ({os.path.getsize(archive) >> 20} MB)")


if __name__ == "__main__":
    main()
