#!/usr/bin/env python3
"""Compile the game's recompiled code and link it to the runtime: abyss.exe, on the player's PC.

  python tools/build_game.py <recomp folder> <runtime folder> <toolchain folder> <program>

  <recomp folder>    tools/make_game.py's output: game*.c and mod_*.c
  <runtime folder>   the release's runtime, compiled: lib/*.a, include/{rt.h,win32_compat.h}, bin/*.dll
  <toolchain folder> llvm-mingw (the compiler the runtime was built with)
  <program>          the abyss.exe to write; the runtime's DLLs (FFmpeg) are copied beside it

The same flags as CMakeLists.txt gives the abyssgame library, and the same libraries as its link of abyss. Objects go
to <recomp folder>/obj; one newer than its source is kept, so a run that stopped resumes. Prints "  k/N" as each file
is compiled.
"""
import concurrent.futures
import ctypes
import glob
import os
import shutil
import subprocess
import sys

# CMakeLists.txt: abyssgame's compile options, and abyss's link (the runtime whole: it registers itself from constructors)
CFLAGS = ["-O2", "-ffp-contract=off", "-w", "-D_FILE_OFFSET_BITS=64", "-D_POSIX_THREAD_SAFE_FUNCTIONS"]
LIBS = ["-Wl,--whole-archive", "libabyssrt.a", "-Wl,--no-whole-archive", "libatrac9.a", "libimgui.a", "libzlibstatic.a",
        "libSDL3.a", "libavcodec.dll.a", "libavutil.dll.a"]
SYSLIBS = ["pthread", "winmm", "ws2_32", "bcrypt", "m", "kernel32", "user32", "gdi32", "imm32", "ole32", "oleaut32",
           "version", "uuid", "advapi32", "setupapi", "shell32", "dinput8", "winspool", "comdlg32"]
JOB_MEMORY = 768 << 20   # the largest part peaks at about 480 MB in clang


def die(msg):
    sys.exit("build_game: " + msg)


def free_memory():
    class MemoryStatus(ctypes.Structure):
        _fields_ = [("length", ctypes.c_ulong), ("load", ctypes.c_ulong), ("total", ctypes.c_ulonglong), ("available", ctypes.c_ulonglong),
                    ("page_total", ctypes.c_ulonglong), ("page_available", ctypes.c_ulonglong), ("virtual_total", ctypes.c_ulonglong),
                    ("virtual_available", ctypes.c_ulonglong), ("extended", ctypes.c_ulonglong)]
    status = MemoryStatus(); status.length = ctypes.sizeof(status)
    return status.available if os.name == "nt" and ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(status)) else 4 << 30


def jobs():
    return max(1, min(os.cpu_count() or 1, free_memory() // JOB_MEMORY))


def main():
    if len(sys.argv) != 5: die(__doc__)
    recomp, runtime, toolchain, program = (os.path.abspath(a) for a in sys.argv[1:])
    sources = sorted(glob.glob(os.path.join(glob.escape(recomp), "game*.c")) + glob.glob(os.path.join(glob.escape(recomp), "mod_*.c")))
    if not any(os.path.basename(s) == "game.c" for s in sources): die(f"no game.c in {recomp}: run tools/make_game.py first")
    bin_dir = os.path.join(toolchain, "bin")
    cc, cxx = os.path.join(bin_dir, "x86_64-w64-mingw32-clang.exe"), os.path.join(bin_dir, "x86_64-w64-mingw32-clang++.exe")
    if not os.path.isfile(cc): die(f"the compiler is not at {cc}")
    include, lib = os.path.join(runtime, "include"), os.path.join(runtime, "lib")
    for f in [os.path.join(include, "rt.h"), os.path.join(include, "win32_compat.h")] + [os.path.join(lib, l) for l in LIBS if l.endswith(".a")]:
        if not os.path.isfile(f): die(f"the runtime is incomplete: {f} is missing")

    obj_dir = os.path.join(recomp, "obj"); os.makedirs(obj_dir, exist_ok=True)
    objects = [os.path.join(obj_dir, os.path.basename(s)[:-2] + ".o") for s in sources]
    todo = [(s, o) for s, o in zip(sources, objects) if not (os.path.isfile(o) and os.path.getmtime(o) > max(os.path.getmtime(s), os.path.getmtime(os.path.join(include, "rt.h"))))]
    env = dict(os.environ, PATH=bin_dir + os.pathsep + os.environ.get("PATH", ""))

    def compile_one(job):
        source, obj = job
        tmp = obj + ".tmp"   # never a half-written object that looks newer than its source
        r = subprocess.run([cc] + CFLAGS + ["-include", os.path.join(include, "win32_compat.h"), "-I", include, "-c", source, "-o", tmp],
                           env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
        if r.returncode: return source, r.stdout
        os.replace(tmp, obj); return source, None

    n, done = len(sources), len(sources) - len(todo)
    print(f"compiling {len(todo)} of {n} files, {jobs()} at a time", flush=True)
    print(f"  {done}/{n}", flush=True)
    with concurrent.futures.ThreadPoolExecutor(jobs()) as pool:
        for source, error in pool.map(compile_one, todo):
            if error:
                sys.stdout.write(error[-4000:]); pool.shutdown(cancel_futures=True)
                die(f"compiling {os.path.basename(source)} failed")
            done += 1; print(f"  {done}/{n}", flush=True)

    print("linking", flush=True)
    os.makedirs(os.path.dirname(program), exist_ok=True)
    tmp = program + ".new.exe"
    r = subprocess.run([cxx, "-O2", "-static"] + objects + ["-L", lib] + [os.path.join(lib, l) if l.endswith(".a") else l for l in LIBS] +
                       ["-l" + l for l in SYSLIBS] + ["-o", tmp], env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
    if r.returncode:
        sys.stdout.write(r.stdout[-4000:]); die("linking failed")
    os.replace(tmp, program)
    for dll in glob.glob(os.path.join(glob.escape(os.path.join(runtime, "bin")), "*.dll")):
        shutil.copyfile(dll, os.path.join(os.path.dirname(program), os.path.basename(dll)))
    print("done:", program, flush=True)


if __name__ == "__main__":
    main()
