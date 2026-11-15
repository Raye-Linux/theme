#!/usr/bin/env python3
#
# Draws the Raye wallpapers into wallpaper/<Package>/contents/images:
#
#   python3 art/wallpapers.py            every wallpaper, 3840x2160 + 1920x1080
#   python3 art/wallpapers.py strata     one of them
#   python3 art/wallpapers.py --small    quick look at 960x540 in /tmp
#
# and the frames of the two animated ones (art/wallpapers_motion.py turns
# them into the .webm files). Needs Pillow and numpy; the output is
# committed. Every picture is seeded, so a second run gives the same bytes.

import json
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOKENS = os.path.join(ROOT, "tokens", "raye-tokens.json")
OUT = os.path.join(ROOT, "wallpaper")

with open(TOKENS, encoding="utf-8") as fh:
    _T = json.load(fh)
VIOLET = _T["color"]["accent"]["night"]


def rgb(h):
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def lerp(a, b, t):
    return tuple(round(x + (y - x) * t) for x, y in zip(a, b))


def finish(img, grain=1.6, seed=1):
    """Fine grain over the whole picture: it hides banding in the soft
    parts and keeps large flat areas from looking printed."""
    a = np.asarray(img, dtype=np.float32)
    rng = np.random.default_rng(seed)
    n = rng.normal(0.0, grain, a.shape[:2])[..., None]
    return Image.fromarray(np.clip(a + n, 0, 255).astype(np.uint8))


def vertical(w, h, stops):
    """A plain top-to-bottom ramp through (position, colour) stops."""
    y = np.linspace(0, 1, h)[:, None]
    out = np.zeros((h, 1, 3), np.float32)
    pos = [p for p, _ in stops]
    for c in range(3):
        out[:, 0, c] = np.interp(y[:, 0], pos, [col[c] for _, col in stops])
    return Image.fromarray(np.repeat(out, w, axis=1).astype(np.uint8))


def vignette(img, strength=0.35, colour=(0, 0, 0)):
    w, h = img.size
    y, x = np.mgrid[0:h, 0:w].astype(np.float32)
    d = np.sqrt(((x - w / 2) / (w / 2)) ** 2 + ((y - h / 2) / (h / 2)) ** 2) / math.sqrt(2)
    m = np.clip(d ** 2 * strength, 0, 1)[..., None]
    a = np.asarray(img, np.float32)
    a = a * (1 - m) + np.array(colour, np.float32) * m
    return Image.fromarray(a.astype(np.uint8))


# --- the height field the contour wallpapers share --------------------

def field(w, h, t=0.0, seed=7):
    """A few broad hills and hollows; t in [0, 1) moves them on a closed
    path, so frame 0 and frame N meet again."""
    rng = np.random.default_rng(seed)
    y, x = np.mgrid[0:h, 0:w].astype(np.float32)
    x /= w
    y /= w
    f = np.zeros((h, w), np.float32)
    for i in range(9):
        cx, cy = rng.uniform(-0.1, 1.1), rng.uniform(-0.1, 0.65)
        r = rng.uniform(0.12, 0.32)
        a = rng.uniform(-1, 1)
        ph = rng.uniform(0, 2 * math.pi)
        cx += 0.035 * math.cos(2 * math.pi * t + ph)
        cy += 0.025 * math.sin(2 * math.pi * t + ph)
        f += a * np.exp(-((x - cx) ** 2 + (y - cy) ** 2) / (2 * r * r))
    # a slow tilt, so the lines run mostly across the screen
    f += 0.8 * y + 0.15 * x
    return f


def contour_mask(f, step, width_px):
    """Pixels near a multiple of step, antialiased by distance."""
    gy, gx = np.gradient(f)
    g = np.sqrt(gx * gx + gy * gy) + 1e-6
    d = np.abs(((f / step) + 0.5) % 1.0 - 0.5) * step / g
    return np.clip(1.0 - (d - width_px / 2), 0, 1), np.floor(f / step)


def strata(w, h, t=0.0, grain=1.6):
    """Night contour lines, one of them in the accent."""
    base = vertical(w, h, [(0, rgb("#16151B")), (1, rgb("#0F0E12"))])
    f = field(w, h, t)
    step = 0.055
    m, band = contour_mask(f, step, 1.1 * w / 1920)
    a = np.asarray(base, np.float32)
    line = np.array(rgb("#34323C"), np.float32)
    a = a * (1 - m[..., None] * 0.9) + line * (m[..., None] * 0.9)
    # one line, about two fifths down, carries the violet
    target = np.floor(np.median(f[int(h * 0.42), :]) / step)
    hi = (band == target).astype(np.float32)
    hi = np.maximum(hi, np.roll(hi, 1, 0))  # the line sits on a band edge
    vio = np.array(rgb(VIOLET), np.float32)
    k = (m * hi)[..., None]
    a = a * (1 - k) + vio * k
    img = Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))
    return finish(vignette(img, 0.25), grain, seed=11)


# --- layered shapes ----------------------------------------------------

def wave(w, h, base_y, amp, waves, phase, t=0.0, wobble=0.0, n=400):
    pts = []
    for i in range(n + 1):
        x = w * i / n
        u = i / n
        y = base_y + amp * math.sin(2 * math.pi * (waves * u + t) + phase)
        y += wobble * amp * math.sin(2 * math.pi * (2.3 * u - t) + phase * 1.7)
        pts.append((x, y))
    return pts


def layered(w, h, layers, sky, t=0.0, shadow=0.0, ss=2, grain=1.4, seed=5):
    """Bands filled from a curve to the bottom, back to front. shadow > 0
    drops a soft shadow on the band behind, as cut paper does."""
    W, H = w * ss, h * ss
    img = sky.resize((W, H), Image.BICUBIC).convert("RGB")
    for colour, base, amp, waves, phase, wob in layers:
        pts = wave(W, H, base * H, amp * H, waves, phase, t, wob)
        poly = pts + [(W, H), (0, H)]
        if shadow:
            sh = Image.new("L", (W, H), 0)
            ImageDraw.Draw(sh).polygon([(x, y - 4 * ss) for x, y in poly], fill=int(255 * shadow))
            sh = sh.filter(ImageFilter.GaussianBlur(14 * ss * w / 1920))
            img = Image.composite(Image.new("RGB", (W, H), (0, 0, 0)), img, sh)
        ImageDraw.Draw(img).polygon(poly, fill=colour)
    img = img.resize((w, h), Image.LANCZOS)
    return finish(img, grain, seed)


def paper(w, h, t=0.0, grain=1.2, ss=2):
    """Light: cut paper in cool greys, the front sheet a pale lilac."""
    sky = vertical(w, h, [(0, rgb("#FAFAFB")), (1, rgb("#ECEBEF"))])
    layers = [
        (rgb("#E4E3E8"), 0.30, 0.050, 1.0, 0.3, 0.30),
        (rgb("#D9D8DF"), 0.44, 0.060, 1.2, 2.1, 0.25),
        (rgb("#CDCBD4"), 0.58, 0.055, 0.8, 4.0, 0.35),
        (rgb("#E8E1F7"), 0.72, 0.050, 1.1, 1.0, 0.20),
        (rgb("#F7F6F9"), 0.86, 0.040, 0.9, 5.2, 0.30),
    ]
    return layered(w, h, layers, sky, t, shadow=0.22, ss=ss, grain=grain, seed=21)


def dune(w, h, grain=1.6):
    """Light: warm sand at noon, a pale sun, one thin violet line of the
    horizon far away."""
    sky = vertical(w, h, [(0, rgb("#F4F2EE")), (0.55, rgb("#ECE8E2")), (1, rgb("#E2DCD3"))])
    d = ImageDraw.Draw(sky)
    r = 0.11 * h
    cx, cy = 0.70 * w, 0.30 * h
    d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=rgb("#FBFAF8"))
    d.line((0, 0.505 * h, w, 0.505 * h), fill=lerp(rgb("#ECE8E2"), rgb(VIOLET), 0.35),
           width=max(1, round(h / 900)))
    layers = [
        (rgb("#E3DDD4"), 0.52, 0.012, 0.6, 0.4, 0.4),
        (rgb("#D9D1C6"), 0.60, 0.035, 0.9, 2.2, 0.5),
        (rgb("#CFC6B9"), 0.71, 0.045, 0.7, 3.9, 0.6),
        (rgb("#C4BAAC"), 0.84, 0.050, 0.55, 5.0, 0.4),
    ]
    return layered(w, h, layers, sky, shadow=0.10, grain=grain, seed=31)


def ridge(w, h, grain=1.8):
    """Night: mountain ridges in blue-grey, a moon, a few stars."""
    sky = vertical(w, h, [(0, rgb("#0D0E14")), (0.6, rgb("#171821")), (1, rgb("#1B1C25"))])
    d = ImageDraw.Draw(sky)
    rng = np.random.default_rng(41)
    for _ in range(int(160 * w / 3840)):
        x, y = rng.uniform(0, w), rng.uniform(0, 0.5 * h) ** 1.0
        s = rng.uniform(0.6, 1.8) * w / 3840
        v = int(rng.uniform(120, 220))
        d.ellipse((x - s, y - s, x + s, y + s), fill=(v, v, min(255, v + 12)))
    r = 0.035 * h
    cx, cy = 0.26 * w, 0.22 * h
    d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=rgb("#E7E6EC"))
    d.ellipse((cx - r + 0.35 * r, cy - r - 0.15 * r, cx + r + 0.35 * r, cy + r - 0.15 * r), fill=rgb("#10111A"))
    rng2 = np.random.default_rng(43)

    def jagged(base, amp, n, seed):
        rr = np.random.default_rng(seed)
        ys = np.cumsum(rr.normal(0, 1, n))
        ys = (ys - ys.min()) / (np.ptp(ys) + 1e-6)
        k = np.ones(5) / 5
        ys = np.convolve(ys, k, mode="same")
        return [(w * i / (n - 1), (base - amp * ys[i]) * h) for i in range(n)]

    img = sky
    for colour, base, amp, seed in [("#22232E", 0.62, 0.22, 1), ("#1C1D27", 0.70, 0.18, 2),
                                    ("#16171F", 0.80, 0.16, 3), ("#101117", 0.92, 0.12, 4)]:
        pts = jagged(base, amp, 90, seed + int(rng2.integers(0, 3)))
        ImageDraw.Draw(img).polygon(pts + [(w, h), (0, h)], fill=rgb(colour))
    return finish(img, grain, seed=45)


def lumen(w, h, grain=1.4):
    """Light: soft paper with one big ring, slightly off-centre, and a
    single short accent arc on it."""
    img = vertical(w, h, [(0, rgb("#F7F7F9")), (1, rgb("#EDECF0"))])
    ss = 2
    big = img.resize((w * ss, h * ss), Image.BICUBIC)
    d = ImageDraw.Draw(big)
    cx, cy, r = 0.62 * w * ss, 0.48 * h * ss, 0.30 * h * ss
    lw = max(2, round(0.006 * h * ss))
    d.ellipse((cx - r, cy - r, cx + r, cy + r), outline=rgb("#D7D5DD"), width=lw)
    r2 = r * 0.72
    d.ellipse((cx - r2, cy - r2, cx + r2, cy + r2), outline=rgb("#E3E1E8"), width=max(1, lw // 2))
    d.arc((cx - r, cy - r, cx + r, cy + r), start=200, end=242, fill=rgb(VIOLET), width=lw)
    img = big.resize((w, h), Image.LANCZOS)
    return finish(img, grain, seed=51)


def lid(d, cx, cy, a, b, upper, colour, width, part=(0.0, 1.0)):
    """One lid of an almond eye: the arc of the circle through both
    corners (cx +- a, cy) that rises b above (or falls b below) them.
    part trims it, 0 = left corner, 1 = right corner."""
    R = (a * a + b * b) / (2 * b)
    oy = cy + (R - b) if upper else cy - (R - b)
    half = math.degrees(math.asin(a / R))
    mid = 270 if upper else 90
    s, e = mid - half, mid + half
    if not upper:
        s, e = e, s
        s, e = s - (s - e) * part[0], s - (s - e) * part[1]
        s, e = e, s
    else:
        s, e = s + (e - s) * part[0], s + (e - s) * part[1]
    d.arc((cx - R, oy - R, cx + R, oy + R), start=s, end=e, fill=colour, width=width)


def night(w, h, grain=1.8):
    """The default: graphite, the Raye eye drawn large and thin on the
    right, most of its lower lid in violet."""
    img = vertical(w, h, [(0, rgb("#17161C")), (1, rgb("#0F0E12"))])
    ss = 2
    big = img.resize((w * ss, h * ss), Image.BICUBIC)
    d = ImageDraw.Draw(big)
    W, H = w * ss, h * ss
    lw = max(2, round(0.0026 * H))
    cx, cy, a, b = 0.64 * W, 0.50 * H, 0.27 * W, 0.20 * H
    grey, dim = rgb("#3A3941"), rgb("#26252C")
    lid(d, cx, cy, a, b, True, grey, lw)
    lid(d, cx, cy, a, b, False, grey, lw)
    lid(d, cx, cy, a, b, False, rgb(VIOLET), lw, (0.12, 0.70))
    lid(d, cx, cy, a * 1.18, b * 1.35, True, dim, max(1, lw // 2))
    r = 0.12 * H
    d.ellipse((cx - r, cy - r, cx + r, cy + r), outline=grey, width=lw)
    r2 = 0.035 * H
    d.ellipse((cx - r2, cy - r2, cx + r2, cy + r2), fill=rgb("#2A2930"))
    img = big.resize((w, h), Image.LANCZOS)
    return finish(vignette(img, 0.30), grain, seed=61)


# --- the packages ------------------------------------------------------

# folder: (Name, Name[fr], draw)
WALLPAPERS = {
    "Raye": ("Raye Night", "Raye Nuit", night),
    "Raye-Strata": ("Strata", "Strates", strata),
    "Raye-Ridge": ("Ridge", "Crêtes", ridge),
    "Raye-Paper": ("Paper", "Papier", paper),
    "Raye-Dune": ("Dune", "Dune", dune),
    "Raye-Lumen": ("Lumen", "Lumen", lumen),
}
SIZES = ((3840, 2160), (1920, 1080))


def metadata(folder, name, name_fr):
    return json.dumps({"KPlugin": {
        "Authors": [{"Name": "techoraye"}],
        "Id": folder,
        "License": "CC-BY-SA-4.0",
        "Name": name,
        "Name[fr]": name_fr,
    }}, indent=4, ensure_ascii=False) + "\n"


def write(folder):
    name, name_fr, draw = WALLPAPERS[folder]
    d = os.path.join(OUT, folder, "contents", "images")
    os.makedirs(d, exist_ok=True)
    if folder == "Raye":
        # the sign-in and lock screens name these two files; drawn without
        # grain the PNG stays small
        big = draw(*SIZES[0], grain=0)
        big.save(os.path.join(d, "3840x2160.png"), optimize=True)
        big.resize(SIZES[1], Image.LANCZOS).save(os.path.join(d, "1920x1080.png"), optimize=True)
    else:
        big = draw(*SIZES[0])
        # JPEG keeps the grain at a sane size; 4:4:4 keeps thin lines sharp
        big.save(os.path.join(d, "3840x2160.jpg"), quality=90, subsampling=0, optimize=True)
        big.resize(SIZES[1], Image.LANCZOS).save(os.path.join(d, "1920x1080.jpg"),
                                                  quality=90, subsampling=0, optimize=True)
    with open(os.path.join(OUT, folder, "metadata.json"), "w", encoding="utf-8") as fh:
        fh.write(metadata(folder, name, name_fr))
    print("wrote", folder)


def main(argv):
    names = [a for a in argv if not a.startswith("-")]
    if "--small" in argv:
        for folder, (_, _, draw) in WALLPAPERS.items():
            if names and not any(n.lower() in folder.lower() for n in names):
                continue
            draw(960, 540).save("/tmp/wp-%s.png" % folder)
            print("/tmp/wp-%s.png" % folder)
        return 0
    for folder in WALLPAPERS:
        if names and not any(n.lower() in folder.lower() for n in names):
            continue
        write(folder)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
