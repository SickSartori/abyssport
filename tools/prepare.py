#!/usr/bin/env python3
"""RecompLauncher's setup step (recomplauncher.json, "prepare"): the launcher's Prepare without its window.

  python tools/prepare.py --install=<release folder> [--game=<copy folder>] [--firmware=<folder or PSVUPDAT.PUP>]

  --install    the unzipped release: python/, tools/, runtime/, toolchain/ (the game is made in its game/ folder, where
               abyssport.exe looks first)
  --game       the player's copy (PCSA00029 or PCSF00012, version 1.00, with sce_sys/package/work.bin); left out, the game
               already made is only compiled again (RecompLauncher runs the step so when the copy is gone: keeps_copy)
  --firmware   the PS Vita system update file PSVUPDAT.PUP (3.74), or the folder RecompLauncher put it in

As the launcher's Prepare does (src/launcher/launcher.cpp, start_setup): tools/make_game.py from the copy and the
firmware, then tools/build_game.py. Prints "progress <0 to 1> <text>" lines, as RecompLauncher reads them; exit 0 when
the game is ready.
"""
import argparse
import glob
import os
import re
import subprocess
import sys


def progress(fraction, text):
    print(f"progress {max(0.0, min(1.0, fraction)):.3f} {text}", flush=True)


def run(command, line_progress):
    """One tool, its lines turned into progress; its last lines go to stderr if it fails."""
    tail = []
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace",
                               env=dict(os.environ, PYTHONIOENCODING="utf-8", PYTHONUTF8="1"))
    for line in process.stdout:
        line = line.rstrip()
        tail = (tail + [line])[-20:]
        line_progress(line)
    if process.wait() != 0:
        sys.stderr.write("\n".join(tail) + "\n")
        sys.exit(process.returncode or 1)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--install", required=True)
    parser.add_argument("--game")
    parser.add_argument("--firmware")
    args = parser.parse_args()
    install = os.path.abspath(args.install)
    python = sys.executable
    library = os.path.join(install, "game", "library")
    recomp = os.path.join(library, "decrypted", "PCSA00029", "recomp")   # (the European copy too: the same code)
    program = os.path.join(install, "game", "abyss.exe")                  # where abyssport.exe finds it (find_game)
    making = bool(args.game)
    if making:
        pup = args.firmware or ""
        if os.path.isdir(pup):
            found = sorted(glob.glob(os.path.join(pup, "*.PUP")) + glob.glob(os.path.join(pup, "*.pup")))
            pup = found[0] if found else ""
        if not os.path.isfile(pup):
            sys.exit("prepare: no firmware file (PSVUPDAT.PUP, the PS Vita system update 3.74)")
        steps = ["Decrypting the firmware", "Decrypting the game", "Checking version 1.00", "Analysing the code", "ARM code to C"]

        def making_line(line):
            m = re.match(r"\[(\d+)/(\d+)\]", line)
            if m and int(m.group(2)) > 0:
                k, n = int(m.group(1)), int(m.group(2))
                progress(0.75 * (k - 1) / n, steps[min(k, len(steps)) - 1])
        progress(0, steps[0])
        run([python, "-u", os.path.join(install, "tools", "make_game.py"), args.game, pup, "--library", library], making_line)
    elif not os.path.isfile(os.path.join(recomp, "game.img")):
        sys.exit("prepare: the game isn't made yet: the player's copy is needed")
    start, span = (0.75, 0.22) if making else (0.0, 0.95)

    def building_line(line):
        m = re.match(r"\s+(\d+)/(\d+)$", line)
        if m and int(m.group(2)) > 0:
            progress(start + span * int(m.group(1)) / int(m.group(2)), "Compiling")
        elif line == "linking":
            progress(start + span, "Linking")
    progress(start, "Compiling")
    run([python, "-u", os.path.join(install, "tools", "build_game.py"), recomp, os.path.join(install, "runtime"),
         os.path.join(install, "toolchain", "llvm-mingw"), program], building_line)
    progress(1, "Ready to play")


if __name__ == "__main__":
    main()
