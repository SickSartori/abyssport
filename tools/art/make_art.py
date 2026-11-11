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

def paint(img, mask, colour):
    """colour (a triple, or an image) laid over img where mask is"""
    m = mask[..., None]
    img[:] = img * (1 - m) + np.asarray(colour, float) * m


def emblem(size=1024, plate=True, detail=True):
    """detail=False: the small icons (48 px and under): no rays, stair lines or sparks, bolder strokes"""
    s = size * 2   # drawn at twice the size, then reduced: smooth edges
    rng = np.random.default_rng(3)
    y, x = np.mgrid[0:s, 0:s]
    img = np.zeros((s, s, 3))
    alpha = np.zeros((s, s))
    cx, base = s / 2, s * 0.72
    sun_y, sun_r = s * 0.40, s * 0.27
    if plate:   # the tile: deep teal to black, warm where the pit lights it, a fine gold edge
        plate_m = mask_of((s, s), lambda d: d.rounded_rectangle([s * 0.02, s * 0.02, s * 0.98, s * 0.98], radius=s * 0.21, fill=255))
        img[:] = gradient(s, s, (24, 40, 44), (4, 7, 9), 1.1)
        add_glow(img, radial(s, s, cx, sun_y, s * 0.55), (90, 70, 30), 0.55)
        add_glow(img, radial(s, s, cx, s * 0.86, s * 0.5, s * 0.22), GOLD, 0.55)
        if detail:
            edge = mask_of((s, s), lambda d: d.rounded_rectangle([s * 0.045, s * 0.045, s * 0.955, s * 0.955], radius=s * 0.18, outline=255, width=int(s * 0.007)))
            paint(img, edge * 0.55, GOLD)
        alpha = plate_m.copy()

    # the sun behind the temple: a soft disc, a ring, faint rays
    disc = np.clip(1 - np.sqrt((x - cx) ** 2 + (y - sun_y) ** 2) / sun_r, 0, 1)
    add_glow(img, disc ** 0.8, (120, 82, 22), 0.9 if plate else 0)
    ring = mask_of((s, s), lambda d: d.ellipse([cx - sun_r, sun_y - sun_r, cx + sun_r, sun_y + sun_r], outline=255, width=int(s * (0.022 if detail else 0.04))))
    shapes = ring.copy()
    if detail:
        angle = np.arctan2(y - sun_y, x - cx)
        dist = np.sqrt((x - cx) ** 2 + (y - sun_y) ** 2)
        fall = np.clip(1 - (dist - sun_r * 1.08) / (sun_r * 0.45), 0, 1) * (dist > sun_r * 1.08)   # fading outwards
        rays = (np.cos(angle * 20) > 0.97) * fall * np.clip((sun_y - y) / (s * 0.08), 0, 1)
        rays = np.asarray(Image.fromarray((rays * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(s * 0.002)), float) / 255
        shapes = np.maximum(shapes, rays * 0.6)

    # the temple: four tiers and a shrine, each tier lit on its top edge, its right half in shade
    tiers, width, height = 4, s * 0.70, s * 0.44
    step_h = height / (tiers + 1)
    temple_m = mask_of((s, s), lambda d: temple(d, cx, base, width, height, tiers))
    gold = gradient(s, s, GOLD_LIGHT, GOLD_DARK, 1.3)
    tops = np.zeros((s, s))
    for i in range(tiers + 1):   # a bright line along the top of each tier and the shrine
        y0 = base - step_h * (i + 1)
        tw = width * (1 - i / (tiers + 0.6)) if i < tiers else width * 0.22
        tops = np.maximum(tops, mask_of((s, s), lambda d, y0=y0, tw=tw: d.rectangle([cx - tw / 2, y0, cx + tw / 2, y0 + s * (0.012 if detail else 0.02)], fill=255)))
    shade = np.where(x > cx, 0.80, 1.0)[..., None]
    stair = mask_of((s, s), lambda d: d.rectangle([cx - s * 0.06, base - step_h * tiers, cx + s * 0.06, base], fill=255))
    door = mask_of((s, s), lambda d: d.polygon([(cx - s * 0.075, base), (cx - s * 0.075, base - s * 0.13), (cx, base - s * 0.19),
                                                 (cx + s * 0.075, base - s * 0.13), (cx + s * 0.075, base)], fill=255))
    body = gold * shade
    if detail:   # the stair: lighter, with a step every so often
        steps = ((y - (base - step_h * tiers)) % (step_h / 3) < s * 0.006) * stair
        body = body + stair[..., None] * 18
        body = body * (1 - steps[..., None] * 0.35)
    body = body * (1 - tops[..., None]) + np.array(GOLD_LIGHT, float) * 1.05 * tops[..., None]
    paint(img, np.clip(shapes - temple_m, 0, 1), gold)
    paint(img, np.clip(temple_m - door, 0, 1), body)

    # the pit below: dark at its rim, light rising from deep inside; the doorway glowing with it
    pit = mask_of((s, s), lambda d: d.polygon([(cx - s * 0.37, base), (cx + s * 0.37, base), (cx + s * 0.21, s * 0.87), (cx - s * 0.21, s * 0.87)], fill=255))
    depth = np.clip((y - base) / (s * 0.15), 0, 1)
    pit_colour = np.array((30, 16, 4), float) * (1 - depth[..., None]) + np.array(GOLD, float) * depth[..., None]
    pit_colour = pit_colour + radial(s, s, cx, s * 0.84, s * 0.22, s * 0.08)[..., None] * np.array(GOLD_LIGHT, float) * 0.8
    paint(img, pit, pit_colour)
    rim = mask_of((s, s), lambda d: d.rectangle([cx - s * 0.37, base, cx + s * 0.37, base + s * 0.012], fill=255))
    paint(img, rim * pit, (60, 36, 8))
    paint(img, door, np.array((24, 12, 3), float))
    add_glow(img, door * radial(s, s, cx, base, s * 0.10, s * 0.20), GOLD_LIGHT, 1.1)
    if detail:   # sparks rising out of the pit
        sparks = Image.new("L", (s, s), 0)
        d = ImageDraw.Draw(sparks)
        for _ in range(18):
            px, py, r = cx + rng.normal(0, s * 0.09), base - rng.uniform(0, s * 0.10), rng.uniform(s * 0.002, s * 0.006)
            d.ellipse([px - r, py - r, px + r, py + r], fill=255)
        add_glow(img, np.asarray(sparks.filter(ImageFilter.GaussianBlur(s * 0.002)), float) / 255, GOLD_LIGHT, 1.0)

    alpha = np.maximum(alpha, np.maximum(np.maximum(shapes, temple_m), pit))
    rgba = np.dstack([np.clip(img, 0, 255), alpha * 255]).astype(np.uint8)
    return Image.fromarray(rgba, "RGBA").resize((size, size), Image.LANCZOS)


def main():
    os.makedirs(OUT, exist_ok=True)
    emblem(512, plate=False).save(os.path.join(OUT, "emblem.png"), optimize=True)
    icon = emblem(1024)
    icon.resize((512, 512), Image.LANCZOS).save(os.path.join(OUT, "icon.png"), optimize=True)
    # the small sizes from the plainer drawing: at 16-48 px the detail turns to noise
    small = emblem(256, detail=False)
    icon.resize((256, 256), Image.LANCZOS).save(os.path.join(ROOT, "res", "abyssport.ico"), sizes=[(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (24, 24), (16, 16)],
                                               append_images=[icon.resize((n, n), Image.LANCZOS) for n in (128, 64)] + [small.resize((n, n), Image.LANCZOS) for n in (48, 32, 24, 16)])
    print("wrote the emblem and the icon")


if __name__ == "__main__":
    main()
