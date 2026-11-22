#!/usr/bin/env python3
"""Make the game's code and data from your own copy: what the build compiles (CMakeLists.txt, ABYSS_RECOMP_DIR).

  python tools/make_game.py <game folder> <firmware update file> [--library <folder>]

  <game folder>            your copy of Uncharted: Golden Abyss version 1.00, the US release (ux0:app/PCSA00029) or the
                           European one (ux0:app/PCSF00012), copied off the Vita with its licence (sce_sys/package/work.bin)
  <firmware update file>   Sony's PS Vita firmware update file (PSVUPDAT.PUP, 3.74): the graphics library the game
                           uses comes from it
  --library                where everything made goes (default: game/library, which git ignores)

Needs Python 3.11 or later and tools/requirements.txt (pip install -r tools/requirements.txt).
The output is <library>/decrypted/PCSA00029/recomp: game*.c, mod_*.c, the .img memory images and fs/app0.
About ten minutes; it then takes the build as long again to compile what changed.

The steps, each a tool of this folder (adapted from Project Nativ, as is the order they run in):
  1. the firmware decrypted (decrypt/vitadecrypt/pup.py)       -> <library>/firmware/<version>
  2. the game decrypted with its licence (vitadecrypt/game.py) -> <library>/decrypted/PCSA00029/app0
  3. its programs kept as ELFs and checked: only version 1.00 fits the game's patches (PCSA00029.toml)
  4. every function found (recompiler/analyze.py): the game, and Sony's libraries loaded with it
  5. ARM code to C (recompiler/recomp.py) and the memory images (recompiler/mkimage.py)
"""
import argparse
import glob
import hashlib
import os
import shutil
import subprocess
import sys
import tomllib

TITLE = "PCSA00029"
TOOLS = os.path.dirname(os.path.abspath(__file__))
RECOMPILER = os.path.join(TOOLS, "recompiler")
MAIN_BASE = 0x81000000


def die(msg):
    sys.exit("make_game: " + msg)


def run(args, what, keep=None):
    """a tool of this folder, its output shown only where it fails; keep: the lines worth showing anyway"""
    env = dict(os.environ, PYTHONPATH=os.path.join(TOOLS, "decrypt"), PYTHONUTF8="1")
    r = subprocess.run([sys.executable] + args, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors="replace")
    if r.returncode:
        sys.stdout.write(r.stdout)
        die(what + " failed")
    for line in r.stdout.splitlines():
        if keep and keep in line: print("   ", line.strip()[:150])


def rmtree(p):
    if os.path.lexists(p):
        if os.path.isdir(p) and not os.path.islink(p) and not (hasattr(os.path, "isjunction") and os.path.isjunction(p)): shutil.rmtree(p)
        else: os.remove(p) if os.path.isfile(p) or os.path.islink(p) else os.rmdir(p)   # (a junction: the link itself)


def link_dir(target, link):
    """a folder link: a junction on Windows (no privilege needed), a symbolic link elsewhere"""
    rmtree(link)
    if os.name == "nt":
        import _winapi
        _winapi.CreateJunction(os.path.abspath(target), os.path.abspath(link))
    else:
        os.symlink(os.path.abspath(target), link)


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""): h.update(b)
    return h.hexdigest()


def main():
    root = os.path.dirname(TOOLS)
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("game"); ap.add_argument("pup")
    ap.add_argument("--library", default=os.path.join(root, "game", "library"))
    a = ap.parse_args()
    for path, what in ((a.game, "the game folder"), (a.pup, "the firmware update file")):
        if not os.path.exists(path): die(f"{what} is not at {path}")
    with open(os.path.join(TOOLS, TITLE + ".toml"), "rb") as f: pack = tomllib.load(f)
    lib = os.path.abspath(a.library)
    W = os.path.join(lib, "decrypted", TITLE); A = os.path.join(W, "analysis"); OUT = os.path.join(W, "recomp")
    for d in (os.path.join(W, "elf"), A, os.path.join(OUT, "fs")): os.makedirs(d, exist_ok=True)

    print("[1/5] the firmware, decrypted", flush=True)
    fw_tmp = os.path.join(lib, "firmware", ".new")
    rmtree(fw_tmp); os.makedirs(fw_tmp)
    run(["-m", "vitadecrypt.pup", os.path.realpath(a.pup), fw_tmp], "decrypting the firmware", keep="PUP:")
    if not os.path.isfile(os.path.join(fw_tmp, "os0", "us", "libgxm_es4.suprx.elf")): die(f"no libgxm_es4 came out of {a.pup}: not a Vita firmware update file?")
    vt = os.path.join(fw_tmp, "version.txt")
    version = open(vt, encoding="utf-8").readline().strip() if os.path.isfile(vt) else "unknown"
    FW = os.path.join(lib, "firmware", version)
    rmtree(FW); os.rename(fw_tmp, FW)

    print("[2/5] the game, decrypted", flush=True)
    app_tmp = os.path.join(W, ".app0.new"); rif = app_tmp + ".rif"
    rmtree(app_tmp); rmtree(rif)
    run(["-m", "vitadecrypt.game", os.path.realpath(a.game), app_tmp, "--license-out", rif], "decrypting the game (is its licence, sce_sys/package/work.bin, there?)", keep="decrypted")
    app0 = os.path.join(W, "app0")
    rmtree(app0); os.rename(app_tmp, app0)
    if os.path.isfile(rif): os.replace(rif, os.path.join(W, "license.rif"))

    print("[3/5] its programs checked against version 1.00", flush=True)
    main_elf = os.path.join(W, "elf", pack["ELF"])
    for f in [os.path.join(app0, "eboot.bin.elf"), os.path.join(app0, pack["ELF"])] + sorted(glob.glob(os.path.join(glob.escape(os.path.join(app0, "sce_module")), "*.elf"))):
        if not os.path.isfile(f): die(f"{f} is missing from the decrypted game")
        shutil.copyfile(f, os.path.join(W, "elf", os.path.basename(f)))
    digest = sha256(main_elf)
    known = {v["sha256"]: name for name, v in pack["versions"].items()}
    if digest not in known:
        die(f"this copy's {pack['ELF']} is not version {', '.join(pack['versions'])} (the US and European releases share it): "
            "another version, or a modified dump. The game's patches are made for that build's addresses and would not fit.")
    with open(os.path.join(W, "version"), "w", newline="\n") as f: f.write(known[digest] + "\n")
    link_dir(app0, os.path.join(OUT, "fs", "app0"))

    # the libraries loaded with the game, each in its own 16 MB slot from 0x88000000: the game's own copy
    # (sce_module) when it ships one, else the firmware's
    mods = pack["MODULES"].split()
    def elf_of(m):
        for f in (os.path.join(W, "elf", m + ".suprx.elf"), os.path.join(FW, "os0", "us", m + ".suprx.elf"), os.path.join(FW, "vs0", "sys", "external", m + ".suprx.elf")):
            if os.path.isfile(f): return f
        die(f"{m}.suprx is neither in the game nor in the firmware")
    base = {m: 0x88000000 + 0x1000000 * i for i, m in enumerate(mods)}
    def links(skip):   # every other module loaded with it: references to their exported variables point at them
        l = ["--link", f"{main_elf}@{MAIN_BASE:#x}"] if skip else []
        for m in mods:
            if m != skip: l += ["--link", f"{elf_of(m)}@{base[m]:#x}"]
        return l

    print("[4/5] the functions found", flush=True)
    seeds = os.path.join(A, "pack_seeds.txt")   # functions the analysis misses, found on runs
    with open(seeds, "w", newline="\n") as f:
        for s in pack["FUNCTION_SEEDS"].split(): f.write(f"{s.split(':')[0]} {s.partition(':')[2]}\n")
    for i, m in enumerate(mods):
        own = os.path.join(RECOMPILER, "seeds", m + ".txt")
        run([os.path.join(RECOMPILER, "analyze.py"), elf_of(m), os.path.join(A, m + ".data.json"), "--base", f"{base[m]:#x}"] + links(m) +
            ["--var-base", f"{0xfff80000 + 0x8000 * i:#x}", "--seeds", seeds] + (["--seeds", f"{own}@{base[m]:#x}"] if os.path.isfile(own) else []),
            f"analysing {m}", keep="functions,")
    run([os.path.join(RECOMPILER, "analyze.py"), main_elf, os.path.join(A, "eboot.data.json")] + links("") + ["--seeds", seeds], "analysing the game", keep="functions,")

    print("[5/5] ARM code to C", flush=True)
    for m in mods:
        # libgxm's sceGxmShaderPatcherRegisterProgram (firmware 3.65/3.74, +0x15c38): the renderer notes each program the game registers
        hooks = ["--hooks", f"{base[m] + 0x15c38:#x}:gxm_register"] if m == "libgxm_es4" else []
        run([os.path.join(RECOMPILER, "recomp.py"), os.path.join(A, m + ".data.json"), os.path.join(OUT, f"mod_{m}.c"), "--split", "2", "--module", m] + hooks, f"recompiling {m}", keep="functions,")
        run([os.path.join(RECOMPILER, "mkimage.py"), os.path.join(A, m + ".data.json"), os.path.join(OUT, m + ".img"), elf_of(m), "--module"], f"making {m}'s image")
    run([os.path.join(RECOMPILER, "recomp.py"), os.path.join(A, "eboot.data.json"), os.path.join(OUT, "game.c"), "--split", str(pack["SPLIT"]),
         "--hooks", pack["HOOKS"], "--insn-hooks", pack["IHOOKS"]], "recompiling the game", keep="functions,")
    run([os.path.join(RECOMPILER, "mkimage.py"), os.path.join(A, "eboot.data.json"), os.path.join(OUT, "game.img"), main_elf], "making the game's image")
    print("done:", OUT)


if __name__ == "__main__":
    main()
