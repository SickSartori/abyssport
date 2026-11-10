#!/usr/bin/env python3
"""Download the compiler abyssport is built with into toolchain/ (which git ignores). Nothing is installed in the
system: delete the folder to be rid of it.

  python tools/get_toolchain.py

  llvm-mingw   Clang for Windows with its own C and C++ libraries: one compiler for the runtime and for the game's
               recompiled code (toolchain/llvm-mingw)
  Ninja        runs the build (toolchain/ninja.exe)

Each is a release of its project on GitHub, pinned here by version and SHA-256.
"""
import hashlib
import os
import shutil
import sys
import urllib.request
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TC = os.path.join(ROOT, "toolchain")
# (name, the file that says it is there, url, sha256, the folder inside the zip to keep as <name> -- None: its files as they are)
PARTS = (
    ("llvm-mingw", os.path.join("llvm-mingw", "bin", "x86_64-w64-mingw32-clang.exe"),
     "https://github.com/mstorsjo/llvm-mingw/releases/download/20261104/llvm-mingw-20261104-ucrt-x86_64.zip",
     "552191b5754c49a5a20723276a6fc56541ea1922c4c50b7b669022333fc1b40f", "llvm-mingw-20261104-ucrt-x86_64"),
    ("ninja", "ninja.exe",
     "https://github.com/ninja-build/ninja/releases/download/v1.13.2/ninja-win.zip",
     "07fc8261b42b20e71d1720b39068c2e14ffcee6396b76fb7a795fb460b78dc65", None),
)


def main():
    os.makedirs(TC, exist_ok=True)
    for name, marker, url, sha, inner in PARTS:
        if os.path.isfile(os.path.join(TC, marker)):
            print(f"{name}: already there"); continue
        zpath = os.path.join(TC, name + ".zip")
        print(f"{name}: downloading {url}", flush=True)
        h = hashlib.sha256()
        with urllib.request.urlopen(url) as r, open(zpath, "wb") as f:
            for b in iter(lambda: r.read(1 << 20), b""): h.update(b); f.write(b)
        if h.hexdigest() != sha:
            os.remove(zpath)
            sys.exit(f"get_toolchain: {name}: the download's SHA-256 is {h.hexdigest()}, not the pinned {sha}: not kept")
        with zipfile.ZipFile(zpath) as z:
            for m in z.namelist():   # (a name in the zip never leaves toolchain/)
                dest = os.path.realpath(os.path.join(TC, m))
                if os.path.commonpath([dest, os.path.realpath(TC)]) != os.path.realpath(TC): sys.exit(f"get_toolchain: {name}: unsafe path in the zip: {m!r}")
            z.extractall(TC)
        os.remove(zpath)
        if inner:
            shutil.rmtree(os.path.join(TC, name), ignore_errors=True)
            os.rename(os.path.join(TC, inner), os.path.join(TC, name))
        print(f"{name}: ready")


if __name__ == "__main__":
    main()
