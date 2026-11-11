#!/usr/bin/env python3
"""abyssport's own artwork, drawn from code: the icon (res/abyssport.ico, res/launcher/emblem.png) and the launcher's
backgrounds (res/launcher/bg_*.jpg), shown until the player's copy of the game gives its own picture.

  python tools/art/make_art.py

Needs Pillow and numpy (a developer's tools: the build uses the PNG and ICO files this writes, which are committed).
Nothing is taken from the game: a stepped temple over a glowing pit, jungle, ruins and a cave, in the launcher's gold.
Deterministic (fixed seeds): running it again writes the same files.
"""
import math
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, "res", "launcher")
GOLD_LIGHT, GOLD, GOLD_DARK = (255, 214, 120), (226, 168, 44), (138, 84, 12)


def lerp(a, b, t):
    return a + (b - a) * t


def gradient(h, w, top, bottom, power=1.0):
    t = np.linspace(0, 1, h)[:, None, None] ** power
    return np.broadcast_to(lerp(np.array(top, float), np.array(bottom, float), t), (h, w, 3)).copy()


def radial(h, w, cx, cy, rx, ry=None):
    """1 at (cx, cy) falling to 0 at the radius, smoothly"""
    y, x = np.mgrid[0:h, 0:w]
    d = np.sqrt(((x - cx) / rx) ** 2 + ((y - cy) / (ry or rx)) ** 2)
    return np.clip(1 - d, 0, 1) ** 2


def add_glow(img, mask, colour, strength=1.0):
    img += mask[..., None] * np.array(colour, float) * strength


def blend(img, mask, colour):
    """paint colour where mask (0..1) is"""
    m = mask[..., None]
    img[:] = img * (1 - m) + np.array(colour, float) * m


def mask_of(size, draw_fn, blur=0):
    m = Image.new("L", size, 0)
    draw_fn(ImageDraw.Draw(m))
    if blur: m = m.filter(ImageFilter.GaussianBlur(blur))
    return np.asarray(m, float) / 255


def ridge(w, rng, base, amp, octaves, smooth=1.0):
    """a skyline: summed sines of random phase, y per column"""
    x = np.arange(w)
    y = np.full(w, float(base))
    for o in range(octaves):
        f = (2 ** o) / w * 2 * math.pi * smooth
        y += amp / (1.7 ** o) * np.sin(x * f * rng.uniform(0.8, 1.3) + rng.uniform(0, 6.28))
    return y


def canopy(w, rng, base, amp, count, radius):
    """a tree line: the top of many round crowns"""
    y = np.full(w, float(base) + amp)
    x = np.arange(w)
    for _ in range(count):
        cx, r = rng.uniform(-radius, w + radius), rng.uniform(radius * 0.5, radius * 1.3)
        top = base + rng.uniform(-amp, amp) * 0.6
        inside = np.abs(x - cx) < r
        y[inside] = np.minimum(y[inside], top + r - np.sqrt(r * r - (x[inside] - cx) ** 2))
    return y


def fill_below(h, w, y_of_x):
    yy = np.arange(h)[:, None]
    return (yy >= y_of_x[None, :]).astype(float)


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


def finish(img, rng, vignette=0.55, grain=3.0):
    h, w, _ = img.shape
    y, x = np.mgrid[0:h, 0:w]
    d = np.sqrt(((x - w / 2) / (w * 0.62)) ** 2 + ((y - h / 2) / (h * 0.62)) ** 2)
    img *= (1 - vignette * np.clip(d - 0.35, 0, 1) ** 1.5)[..., None]
    img += rng.normal(0, grain, img.shape)   # (grain: no banding in the dark gradients)
    return Image.fromarray(np.clip(img, 0, 255).astype(np.uint8))


def motes(img, rng, count, area, colour, size=(1, 3)):
    h, w, _ = img.shape
    layer = Image.new("L", (w, h), 0)
    d = ImageDraw.Draw(layer)
    x0, y0, x1, y1 = area
    for _ in range(count):
        x, y, r = rng.uniform(x0, x1), rng.uniform(y0, y1), rng.uniform(*size)
        d.ellipse([x - r, y - r, x + r, y + r], fill=int(rng.uniform(90, 255)))
    m = np.asarray(layer.filter(ImageFilter.GaussianBlur(1.2)), float) / 255
    add_glow(img, m, colour, 0.9)


# --- the backgrounds: 1920x1080, darker on the left where the launcher's menu sits ---

def bg_jungle():
    """dusk over the jungle, a temple on the horizon"""
    w, h = 1920, 1080
    rng = np.random.default_rng(7)
    img = gradient(h, w, (8, 20, 24), (92, 58, 22), 1.6)
    add_glow(img, radial(h, w, 1300, 640, 900, 420), (255, 170, 70), 0.55)
    add_glow(img, radial(h, w, 1300, 610, 140), (255, 230, 170), 0.6)
    motes(img, rng, 220, (0, 0, w, 520), (200, 210, 220), (0.4, 1.2))   # first stars
    hill_line = ridge(w, rng, 650, 22, 4)
    far = mask_of((w, h), lambda d: temple(d, 1300, hill_line[1300] + 6, 380, 230, 5), 1)
    hills = fill_below(h, w, hill_line)
    blend(img, np.maximum(far, hills), (40, 30, 24))
    add_glow(img, radial(h, w, 1300, 660, 1100, 120), (255, 180, 90), 0.25)   # haze over the plain
    for base, amp, colour, radius, blur in ((700, 40, (20, 26, 24), 70, 2), (790, 50, (12, 17, 15), 110, 1), (900, 60, (6, 9, 8), 170, 0)):
        m = fill_below(h, w, canopy(w, rng, base, amp, int(w / radius * 3), radius))
        if blur: m = np.asarray(Image.fromarray((m * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(blur)), float) / 255
        blend(img, m, colour)
        add_glow(img, radial(h, w, 1300, base, 1200, 60), (255, 160, 70), 0.10)   # mist between the layers
    # leaves hanging into the top left corner, where the launcher's title is: big dark fronds, out of focus
    def fronds(d):
        for _ in range(9):
            x0, y0 = rng.uniform(-120, 560), rng.uniform(-80, 40)
            angle, length = rng.uniform(0.9, 1.9), rng.uniform(260, 520)
            for k in range(14):
                t = k / 14
                px, py = x0 + math.cos(angle) * length * t, y0 + math.sin(angle) * length * t + 90 * t * t
                r = 46 * math.sin(math.pi * (t + 0.08)) + 6
                d.ellipse([px - r * 1.5, py - r * 0.6, px + r * 1.5, py + r * 0.6], fill=255)
    blend(img, mask_of((w, h), fronds, 5), (4, 7, 6))
    return finish(img, rng, 0.65)


def bg_abyss():
    """inside a cave, light falling into a golden pit"""
    w, h = 1920, 1080
    rng = np.random.default_rng(11)
    img = gradient(h, w, (10, 13, 16), (4, 6, 8))
    add_glow(img, radial(h, w, 1240, 980, 760, 300), GOLD, 0.85)
    add_glow(img, radial(h, w, 1240, 1010, 300, 110), GOLD_LIGHT, 0.9)
    shaft = mask_of((w, h), lambda d: d.polygon([(1120, 0), (1300, 0), (1420, 1000), (1060, 1000)], fill=255), 60)
    add_glow(img, shaft * np.linspace(0.55, 0.15, h)[:, None], (255, 220, 160), 0.6)
    # the walls: two jagged masses with lit edges
    yy = np.arange(h)
    left = 700 + 160 * np.sin(yy / 140) + 60 * np.sin(yy / 37 + 1) + 25 * rng.standard_normal(h).cumsum() / 30
    right = 1640 - 120 * np.sin(yy / 170 + 2) - 40 * np.sin(yy / 29) - 20 * rng.standard_normal(h).cumsum() / 30
    x = np.arange(w)[None, :]
    walls = ((x < left[:, None]) | (x > right[:, None])).astype(float)
    walls = np.asarray(Image.fromarray((walls * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(2)), float) / 255
    edge = np.clip(walls - np.asarray(Image.fromarray((walls * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(14)), float) / 255, 0, 1)
    blend(img, walls, (9, 10, 11))
    add_glow(img, edge * radial(h, w, 1240, 900, 1100, 800), GOLD, 1.4)
    steps = mask_of((w, h), lambda d: [d.rectangle([1240 - 330 + i * 30, 1000 - i * 26, 1240 + 330 - i * 30, 1080], fill=255) for i in range(5)])
    blend(img, steps * 0.92, (14, 11, 8))
    add_glow(img, radial(h, w, 1240, 900, 260, 60), GOLD_LIGHT, 0.35)
    motes(img, rng, 380, (900, 100, 1600, 1000), (255, 200, 110), (0.6, 2.4))
    return finish(img, rng, 0.5)


def bg_ruins():
    """a broken colonnade in the moonlit mist"""
    w, h = 1920, 1080
    rng = np.random.default_rng(23)
    img = gradient(h, w, (14, 22, 30), (30, 34, 34), 0.8)
    add_glow(img, radial(h, w, 1450, 230, 380), (150, 175, 190), 0.35)
    add_glow(img, radial(h, w, 1450, 230, 60), (230, 235, 225), 0.9)
    motes(img, rng, 260, (0, 0, w, 600), (190, 205, 215), (0.4, 1.1))
    for depth, (shade, scale, blur) in enumerate((((40, 46, 50), 0.55, 3), ((22, 26, 28), 0.8, 1), ((8, 10, 11), 1.15, 0))):
        def cols(d, scale=scale):
            ground = 820 + depth * 90
            x = 700 + depth * 60
            while x < w + 100:
                cw = 54 * scale
                top = ground - rng.uniform(240, 520) * scale
                if rng.random() < 0.25: top = ground - rng.uniform(60, 160) * scale   # a broken one
                d.rectangle([x, top, x + cw, ground], fill=255)
                d.rectangle([x - 10 * scale, top - 18 * scale, x + cw + 10 * scale, top], fill=255)   # its capital
                if rng.random() < 0.35 and x + cw * 4 < w + 100:   # a lintel to the next
                    d.rectangle([x - 10 * scale, top - 40 * scale, x + cw * 3.4, top - 18 * scale], fill=255)
                x += cw * rng.uniform(2.4, 3.6)
            d.rectangle([0, ground, w, h], fill=255)
        blend(img, mask_of((w, h), cols, blur), shade)
        add_glow(img, radial(h, w, 1300, 760 + depth * 90, 1400, 70), (170, 180, 175), 0.18)   # mist on the ground
    blocks = mask_of((w, h), lambda d: [d.rectangle([x, 1030 - s, x + s * 1.6, 1080], fill=255) for x, s in ((520, 70), (640, 40), (1720, 90))])
    blend(img, blocks, (5, 6, 7))
    return finish(img, rng, 0.6)


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
    for name, make in (("bg_jungle", bg_jungle), ("bg_abyss", bg_abyss), ("bg_ruins", bg_ruins)):
        make().save(os.path.join(OUT, name + ".jpg"), quality=90, optimize=True, subsampling=0)   # (PNG: 3 MB each, for the grain)
        print("wrote", name)
    emblem(512, plate=False).save(os.path.join(OUT, "emblem.png"), optimize=True)
    icon = emblem(1024)
    icon.resize((512, 512), Image.LANCZOS).save(os.path.join(OUT, "icon.png"), optimize=True)
    icon.save(os.path.join(ROOT, "res", "abyssport.ico"), sizes=[(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (24, 24), (16, 16)])
    print("wrote the emblem and the icon")


if __name__ == "__main__":
    main()
