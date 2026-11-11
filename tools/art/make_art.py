#!/usr/bin/env python3
"""abyssport's own artwork, drawn from code: the icon (res/abyssport.ico, res/launcher/icon.png) and the emblem beside
the port's name in the launcher (res/launcher/emblem.png). The launcher's backgrounds (res/launcher/bg_*.jpg) are not
made here: they were generated with Magnific (THIRD_PARTY.md).

  python tools/art/make_art.py

Needs Pillow and numpy (a developer's tools: the build uses the PNG and ICO files this writes, which are committed).
Nothing is taken from the game: a stepped temple over a glowing pit, in the launcher's gold.
Deterministic (fixed seeds): running it again writes the same files.
"""
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, "res", "launcher")
GOLD_LIGHT, GOLD, GOLD_DARK = (255, 214, 120), (226, 168, 44), (138, 84, 12)


def gradient(h, w, top, bottom, power=1.0):
    t = np.linspace(0, 1, h)[:, None, None] ** power
    top, bottom = np.array(top, float), np.array(bottom, float)
    return np.broadcast_to(top + (bottom - top) * t, (h, w, 3)).copy()


def radial(h, w, cx, cy, rx, ry=None):
    """1 at (cx, cy) falling to 0 at the radius, smoothly"""
    y, x = np.mgrid[0:h, 0:w]
    d = np.sqrt(((x - cx) / rx) ** 2 + ((y - cy) / (ry or rx)) ** 2)
    return np.clip(1 - d, 0, 1) ** 2


def add_glow(img, mask, colour, strength=1.0):
    img += mask[..., None] * np.array(colour, float) * strength


def mask_of(size, draw_fn, blur=0):
    m = Image.new("L", size, 0)
    draw_fn(ImageDraw.Draw(m))
    if blur: m = m.filter(ImageFilter.GaussianBlur(blur))
    return np.asarray(m, float) / 255


def temple(draw, cx, base, width, height, tiers, shrine=True, fill=255):
    """a stepped pyramid with a stair down its middle"""
    step_h = height / (tiers + (1 if shrine else 0))
    for i in range(tiers):
        tw = width * (1 - i / (tiers + 0.6))
        y0 = base - step_h * (i + 1)
        draw.rectangle([cx - tw / 2, y0, cx + tw / 2, base - step_h * i], fill=fill)
    if shrine:
        sw = width * 0.22
        draw.rectangle([cx - sw / 2, base - step_h * (tiers + 1), cx + sw / 2, base - step_h * tiers], fill=fill)


# --- the emblem and the icon: a stepped temple over a glowing pit, in gold ---

def emblem(size=1024, plate=True):
    s = size * 2   # drawn at twice the size, then reduced: smooth edges
    rng = np.random.default_rng(3)
    img = np.zeros((s, s, 3))
    alpha = np.zeros((s, s))
    if plate:
        r = s * 0.21
        plate_m = mask_of((s, s), lambda d: d.rounded_rectangle([s * 0.02, s * 0.02, s * 0.98, s * 0.98], radius=r, fill=255))
        img[:] = gradient(s, s, (22, 34, 38), (5, 8, 10), 1.2)
        add_glow(img, radial(s, s, s / 2, s * 0.78, s * 0.42, s * 0.2), GOLD, 0.65)
        alpha = plate_m.copy()
    cx, base = s / 2, s * 0.74
    ring = mask_of((s, s), lambda d: d.ellipse([cx - s * 0.30, s * 0.10, cx + s * 0.30, s * 0.70], outline=255, width=int(s * 0.028)))
    temple_m = mask_of((s, s), lambda d: temple(d, cx, base, s * 0.70, s * 0.46, 4))
    door = mask_of((s, s), lambda d: d.polygon([(cx - s * 0.075, base), (cx - s * 0.075, base - s * 0.15), (cx, base - s * 0.21),
                                                 (cx + s * 0.075, base - s * 0.15), (cx + s * 0.075, base)], fill=255))
    pit = mask_of((s, s), lambda d: d.polygon([(cx - s * 0.36, base), (cx + s * 0.36, base), (cx + s * 0.20, s * 0.86), (cx - s * 0.20, s * 0.86)], fill=255))
    ring = ring * (1 - mask_of((s, s), lambda d: d.rectangle([0, s * 0.40, s, s], fill=255), 0))   # only the sun's upper arc
    shape = np.clip(np.maximum(ring, temple_m) - door, 0, 1)
    shading = gradient(s, s, GOLD_LIGHT, GOLD_DARK, 1.0)
    shading = shading * (0.85 + 0.15 * radial(s, s, cx * 0.8, s * 0.3, s * 0.8)[..., None])
    img = img * (1 - shape[..., None]) + shading * shape[..., None]
    pit_light = pit * np.clip((np.arange(s)[:, None] - base) / (s * 0.12), 0, 1)
    img = img * (1 - pit[..., None]) + (np.array(GOLD, float) * (1 - pit_light[..., None]) * 0.9 + np.array((40, 20, 4), float) * pit_light[..., None]) * pit[..., None]
    add_glow(img, door * radial(s, s, cx, base, s * 0.12, s * 0.2), GOLD_LIGHT, 0.85)
    alpha = np.maximum(alpha, np.maximum(shape, np.maximum(pit, door)))
    rgba = np.dstack([np.clip(img, 0, 255), alpha * 255]).astype(np.uint8)
    return Image.fromarray(rgba, "RGBA").resize((size, size), Image.LANCZOS)


def main():
    os.makedirs(OUT, exist_ok=True)
    emblem(512, plate=False).save(os.path.join(OUT, "emblem.png"), optimize=True)
    icon = emblem(1024)
    icon.resize((512, 512), Image.LANCZOS).save(os.path.join(OUT, "icon.png"), optimize=True)
    icon.save(os.path.join(ROOT, "res", "abyssport.ico"), sizes=[(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (24, 24), (16, 16)])
    print("wrote the emblem and the icon")


if __name__ == "__main__":
    main()
