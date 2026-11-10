#!/usr/bin/env python3
"""Make the game's code and data from your own copy: what the build compiles (CMakeLists.txt, ABYSS_RECOMP_DIR).

  python tools/make_game.py <game folder> <firmware update file> [--library <folder>] [--nativ <checkout>]

  <game folder>            your copy of Uncharted: Golden Abyss, US release PCSA00029 version 1.00: ux0:app/PCSA00029
                           copied off the Vita, with its licence (sce_sys/package/work.bin)
  <firmware update file>   Sony's PS Vita firmware update file (PSVUPDAT.PUP, 3.74): the graphics library the game
                           uses comes from it
  --library                where everything made goes (default: game/library, which git ignores)
  --nativ                  a checkout of Project Nativ (default: external/nativ): its decrypter, analyser and
                           recompiler do the work; this script only runs them in order

Needs Python 3.11 or later with Nativ's two requirements (pip install -r <nativ>/requirements.txt).
The output is <library>/decrypted/PCSA00029/recomp: game*.c, mod_*.c, the .img memory images and fs/app0.
About ten minutes; it then takes the build as long again to compile what changed.
"""
import argparse
import os
import subprocess
import sys

TITLE = "PCSA00029"


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("game"); ap.add_argument("pup")
    ap.add_argument("--library", default=os.path.join(root, "game", "library"))
    ap.add_argument("--nativ", default=os.path.join(root, "external", "nativ"))
    a = ap.parse_args()
    tool = os.path.join(a.nativ, "pipeline", "vitarecomp.py")
    for path, what in ((tool, "Project Nativ's pipeline (--nativ)"), (a.game, "the game folder"), (a.pup, "the firmware update file")):
        if not os.path.exists(path): sys.exit(f"make_game: {what} is not at {path}")
    env = dict(os.environ, ATIV_DUMP=os.path.abspath(a.library), PYTHONUTF8="1")
    steps = (
        ("the firmware, decrypted", ["firmware", os.path.abspath(a.pup)]),
        ("the game, decrypted", ["add", os.path.abspath(a.game), "--no-build"]),
        ("its programs checked against version 1.00", ["import", TITLE]),
        ("the functions found", ["analyze", TITLE]),
        ("ARM code to C", ["recompile", TITLE]),
    )
    for n, (what, args) in enumerate(steps, 1):
        print(f"[{n}/{len(steps)}] {what}", flush=True)
        r = subprocess.run([sys.executable, tool] + args, env=env)
        if r.returncode: sys.exit(f"make_game: step {n} ({' '.join(args[:1])}) failed")
    print("done:", os.path.join(env["ATIV_DUMP"], "decrypted", TITLE, "recomp"))


if __name__ == "__main__":
    main()
