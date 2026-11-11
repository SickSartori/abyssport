#!/usr/bin/env python3
"""The controller drawn in the launcher's "How to play" (res/launcher/pad.png), from a controller drawing in SVG.

  python tools/art/make_pad.py <controller.svg>

The drawing is one standard controller for the DualSense and the Xbox alike: the launcher draws the face buttons'
symbols (the PlayStation shapes or A B X Y) over its four buttons. Here its square background and the disc behind it
go, the four coloured buttons become plain grey, the navy outlines turn to the launcher's warm black, and it is cut to
the controller and written 512 pixels wide with ImageMagick's own SVG renderer (magick, on PATH).
"""
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, "res", "launcher")
VIEW = (760, 1150, 2480, 2020)   # the controller, with room above it for the launcher's shoulder buttons: x, y, w, h


def main():
    if len(sys.argv) != 2: sys.exit(__doc__)
    s = open(sys.argv[1], encoding="utf-8").read()
    s = re.sub(r'<rect style="fill:#0F227A;" width="4000" height="4000"/>', "", s)                 # the square background
    s = re.sub(r'<circle style="fill:#0BCBFF;"[^>]*/>', "", s)                                    # the disc behind
    for colour in ("#E5C62C", "#E52C5C", "#5E7DEF", "#55BC5C"):                                    # Y B X A: plain buttons
        s = s.replace(f"fill:{colour};", "fill:#D0D4D8;")
    s = s.replace("#0F227A", "#1A1714").replace("#102770", "#1A1714")                               # outlines and shading
    s = re.sub(r'viewBox="[^"]*"', 'viewBox="%d %d %d %d"' % VIEW, s, count=1)
    s = re.sub(r'style="enable-background:new [^"]*"', "", s, count=1)
    png = os.path.join(OUT, "pad.png")
    with tempfile.TemporaryDirectory() as work:
        svg = os.path.join(work, "pad.svg")
        open(svg, "w", encoding="utf-8", newline="\n").write(s)
        subprocess.run(["magick", "-background", "none", "-density", "40", "MSVG:" + svg, "-resize", "512x", png], check=True)
    print("wrote", png)
    # the drawing's own bumpers, as a mask the launcher lights up (left half: L1/LB, right half: R1/RB): their bluish
    # greys (slate, its lit edge) at the top, left and right of the middle (the same grey, between them)
    from PIL import Image
    pad = Image.open(png).convert("RGBA")
    mask = Image.new("RGBA", pad.size, (255, 255, 255, 0))
    w, h = pad.size
    for y in range(int(h * 0.28)):
        for x in range(w):
            r, g, b, a = pad.getpixel((x, y))
            slate = a > 128 and 60 < r < 200 and b - r >= 4
            if slate and (x < w * 0.36 or x > w * 0.64): mask.putpixel((x, y), (255, 255, 255, a))
    mask.save(os.path.join(OUT, "pad_bumpers.png"), optimize=True)
    print("wrote the bumpers' mask")


if __name__ == "__main__":
    main()
