#!/usr/bin/env python3
#
# Pictures of the boot screen (plymouth/raye/):
#
#   eye.png          the eye at rest, without the scanline slices: on a
#                    still screen they read as a broken picture. Same box
#                    as with the scanlines, so the almond is in the
#                    middle of the picture and as wide as on the sign-in
#                    splash
#   glow.png         the soft light behind the eye, breathing with the ring
#   ring-<n>.png     the progress ring, n px across: RING_FRAMES frames of
#                    one loop on a grid of RING_COLS, for the sizes in
#                    RING_SIZES (the script picks one from the screen)
#   ring-<n>-track.png  the faint circle under it, same box as a frame
#   bullet.png       one typed character in the password field
#   field.png        the password field, 360x40, radius 4, accent line
#   background.png   the night gradient with its two glows, drawn small
#                    and scaled to the screen by the script
#   tpm-marker.png   1x1, copied to tpm.png by the initramfs hook
#
#   python3 art/plymouth.py
#
# The output is committed, so the package build doesn't need Pillow.
# The words are not pictures: the hook puts Inter in the initramfs and
# the script draws them.

import os
import sys
from math import cos, radians, sin

from PIL import Image, ImageChops, ImageDraw, ImageFilter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import eye  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "plymouth", "raye")

TOP = (0x0e, 0x0a, 0x17)
BOTTOM = (0x1a, 0x10, 0x28)
VIOLET = (0x7b, 0x3c, 0xff)
GLOW = (0xc0, 0x40, 0xff)
TEXT = (0xf3, 0xea, 0xff)

# eye.png: the box goes as far left of the almond as the scanlines go
# right of it, so the almond sits in the middle. The script needs the
# share of the picture the almond takes: EYE_WIDTH in raye.script.in.
EYE_REACH = eye.A + 88
EYE_PAD = 0.04
EYE_WIDTH = 2 * eye.A / (2 * EYE_REACH * (1 + 2 * EYE_PAD))


def save(img: Image.Image, name: str) -> None:
    os.makedirs(OUT, exist_ok=True)
    img.save(os.path.join(OUT, name), optimize=True)


def eye_picture(size: int = 768) -> Image.Image:
    img = eye.matte(eye.mark((0, 0, 0)), eye.mark((255, 255, 255)))
    side = 2 * EYE_REACH * (1 + 2 * EYE_PAD)
    s = eye.S
    box = (round((eye.CX - side / 2) * s), round((eye.CY - side / 2) * s),
           round((eye.CX + side / 2) * s), round((eye.CY + side / 2) * s))
    # the crop goes past the left edge of the drawing: pad it first
    canvas = Image.new("RGBA", (eye.W * s * 2, eye.H * s * 2), (0, 0, 0, 0))
    off = (eye.W * s // 2, eye.H * s // 2)
    canvas.paste(img, off)
    box = (box[0] + off[0], box[1] + off[1], box[2] + off[0], box[3] + off[1])
    return canvas.crop(box).resize((size, size), Image.LANCZOS)


def radial(size: int, colour: tuple, alpha: float, falloff: float) -> Image.Image:
    """A round light: full alpha in the middle, fading to nothing."""
    img = Image.new("RGBA", (size, size), colour + (0,))
    a = Image.new("L", (size, size), 0)
    c = (size - 1) / 2
    px = a.load()
    for y in range(size):
        for x in range(size):
            d = ((x - c) ** 2 + (y - c) ** 2) ** 0.5 / c
            v = max(0.0, 1 - d) ** falloff
            px[x, y] = round(255 * alpha * v)
    img.putalpha(a)
    return img


def glow_picture() -> Image.Image:
    outer = radial(256, GLOW, 0.30, 2.2)
    inner = radial(256, VIOLET, 0.35, 3.0)
    return Image.alpha_composite(outer, inner)


# The progress ring: one arc on a faint track. Its head runs ahead
# while the tail waits, then the tail catches up, and the whole thing
# turns all along, so it never stops. One loop is 2 s, 120 frames at
# 60 Hz; it ends where it began, shifted by whole turns.
RING_SIZES = (36, 48, 64, 96)
RING_FRAMES = 120
RING_COLS = 12
RING_SHORT = 12        # degrees, the arc at its shortest
RING_STRETCH = 270     # degrees the head gains, then the tail
RING_SPIN = 1.25       # turns per loop on top of that: 450 + 270 = 720
RING_STROKE = 0.105    # of the size
RING_PAD = 0.2         # of the size, around it, for the glow


def bezier(x1: float, y1: float, x2: float, y2: float):
    """CSS cubic-bezier(x1, y1, x2, y2) as a function of 0..1."""
    def at(a: float, b: float, t: float) -> float:
        return 3 * a * t * (1 - t) ** 2 + 3 * b * t * t * (1 - t) + t ** 3

    def f(x: float) -> float:
        if x <= 0:
            return 0.0
        if x >= 1:
            return 1.0
        lo, hi = 0.0, 1.0
        for _ in range(40):
            mid = (lo + hi) / 2
            if at(x1, x2, mid) < x:
                lo = mid
            else:
                hi = mid
        return at(y1, y2, (lo + hi) / 2)
    return f


RING_EASE = bezier(0.6, 0.0, 0.3, 1.0)


def ring_arc(i: int) -> tuple[float, float]:
    """Tail and head of frame i, in degrees clockwise from 12 o'clock.
    The head moves over the first 60 % of the loop, the tail over the
    last 60 %: in the middle both move, so there is no pause."""
    u = i / RING_FRAMES
    turn = 360 * RING_SPIN * u
    head = turn + RING_SHORT + RING_STRETCH * RING_EASE(u / 0.6)
    tail = turn + RING_STRETCH * RING_EASE((u - 0.4) / 0.6)
    return tail, head


def mix(a: tuple, b: tuple, t: float) -> tuple:
    return tuple(round(p + (q - p) * t) for p, q in zip(a, b))


def ring_frame(size: int, tail: float, head: float, k: int = 4) -> Image.Image:
    """One frame, size px across plus the pad. Violet at the tail to
    magenta at the head, round ends, a soft magenta light under it."""
    pad = round(size * RING_PAD)
    side = size + 2 * pad
    n = side * k
    w = size * RING_STROKE * k
    c = n / 2
    outer = size * k / 2
    mid = outer - w / 2
    box = (c - outer, c - outer, c + outer, c + outer)

    def point(deg: float) -> tuple:
        a = radians(deg)
        return c + mid * sin(a), c - mid * cos(a)

    def cap(d: ImageDraw.ImageDraw, deg: float, fill) -> None:
        x, y = point(deg)
        d.ellipse((x - w / 2, y - w / 2, x + w / 2, y + w / 2), fill=fill)

    # PIL measures from 3 o'clock; the ring from 12
    mask = Image.new("L", (n, n), 0)
    d = ImageDraw.Draw(mask)
    d.arc(box, tail - 90, head - 90, fill=255, width=round(w))
    cap(d, tail, 255)
    cap(d, head, 255)

    colour = Image.new("RGB", (n, n), VIOLET)
    d = ImageDraw.Draw(colour)
    steps = max(12, round((head - tail) / 2))
    for j in range(steps):
        a = tail + (head - tail) * j / steps
        b = tail + (head - tail) * (j + 1) / steps
        d.arc(box, a - 90, b - 90 + 1, fill=mix(VIOLET, GLOW, j / (steps - 1)),
              width=round(w) + 2 * k)
    cap(d, head, GLOW)
    arc = colour.convert("RGBA")
    arc.putalpha(mask)
    arc = arc.convert("RGBa").resize((side, side), Image.LANCZOS).convert("RGBA")

    light = Image.new("RGBA", (side, side), GLOW + (0,))
    light.putalpha(arc.getchannel("A").point(lambda v: round(v * 0.55)))
    light = light.filter(ImageFilter.GaussianBlur(size * RING_STROKE * 0.9))
    return Image.alpha_composite(light, arc)


def ring_track(size: int, k: int = 4) -> Image.Image:
    """The faint full circle under the arc, same box as a frame. It never
    moves, so it is its own picture and not in every frame."""
    pad = round(size * RING_PAD)
    side = size + 2 * pad
    n = side * k
    w = round(size * RING_STROKE * k)
    c = n / 2
    outer = size * k / 2
    a = Image.new("L", (n, n), 0)
    ImageDraw.Draw(a).ellipse((c - outer, c - outer, c + outer, c + outer),
                              outline=255, width=w)
    img = Image.new("RGBA", (n, n), VIOLET + (0,))
    img.putalpha(a.point(lambda v: round(v * 0.16)))
    return img.convert("RGBa").resize((side, side), Image.LANCZOS).convert("RGBA")


def ring_sheet(size: int) -> Image.Image:
    """All the frames on a grid, cut back apart by the script. 256
    colours: as good to the eye, and the boot image stays small."""
    frames = [ring_frame(size, *ring_arc(i)) for i in range(RING_FRAMES)]
    side = frames[0].width
    rows = -(-RING_FRAMES // RING_COLS)
    sheet = Image.new("RGBA", (side * RING_COLS, side * rows), (0, 0, 0, 0))
    for i, f in enumerate(frames):
        sheet.paste(f, ((i % RING_COLS) * side, (i // RING_COLS) * side))
    return sheet.quantize(256, method=Image.Quantize.LIBIMAGEQUANT)


def bullet_picture() -> Image.Image:
    k = 8
    img = Image.new("RGBA", (10 * k, 10 * k), (0, 0, 0, 0))
    ImageDraw.Draw(img).ellipse((k, k, 9 * k - 1, 9 * k - 1), fill=TEXT + (255,))
    return img.resize((10, 10), Image.LANCZOS)


def field_picture() -> Image.Image:
    """Text box of the tokens: fill white 6 %, stroke 10 %, radius 4,
    2 px accent line at the bottom (the field always has the focus)."""
    w, h, k = 360, 40, 8
    img = Image.new("RGBA", (w * k, h * k), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((0, 0, w * k - 1, h * k - 1), radius=4 * k,
                        fill=(255, 255, 255, round(255 * 0.10)))
    d.rounded_rectangle((k, k, w * k - k - 1, h * k - k - 1), radius=3 * k,
                        fill=(255, 255, 255, round(255 * 0.06)))
    # the accent line follows the rounded corners at the bottom
    mask = Image.new("L", (w * k, h * k), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, w * k - 1, h * k - 1),
                                           radius=4 * k, fill=255)
    line = Image.new("RGBA", (w * k, h * k), (0, 0, 0, 0))
    ImageDraw.Draw(line).rectangle((0, (h - 2) * k, w * k, h * k), fill=VIOLET + (255,))
    line.putalpha(ImageChops.multiply(line.getchannel("A"), mask))
    img = Image.alpha_composite(img, line)
    return img.resize((w, h), Image.LANCZOS)


def background_picture(w: int = 480, h: int = 270) -> Image.Image:
    """Vertical gradient, a violet glow in the top left corner (14 %) and
    a magenta one in the bottom right (8 %), both fading out well before
    the middle. Small: the script scales it to the screen."""
    img = Image.new("RGB", (w, h))
    px = img.load()
    reach = 0.75 * (w * w + h * h) ** 0.5
    for y in range(h):
        t = y / (h - 1)
        base = [a + (b - a) * t for a, b in zip(TOP, BOTTOM)]
        for x in range(w):
            c = base
            for colour, alpha, (gx, gy) in ((VIOLET, 0.14, (0, 0)),
                                            (GLOW, 0.08, (w, h))):
                d = ((x - gx) ** 2 + (y - gy) ** 2) ** 0.5 / reach
                k = alpha * max(0.0, 1 - d) ** 2
                c = [p + (q - p) * k for p, q in zip(c, colour)]
            px[x, y] = tuple(round(v) for v in c)
    return img


def main() -> None:
    save(eye_picture(), "eye.png")
    save(glow_picture(), "glow.png")
    for n in RING_SIZES:
        save(ring_sheet(n), f"ring-{n}.png")
        save(ring_track(n), f"ring-{n}-track.png")
    save(bullet_picture(), "bullet.png")
    save(field_picture(), "field.png")
    save(background_picture(), "background.png")
    save(Image.new("RGBA", (1, 1), (0, 0, 0, 0)), "tpm-marker.png")
    print(f"EYE_WIDTH = {EYE_WIDTH:.4f}")


if __name__ == "__main__":
    main()
