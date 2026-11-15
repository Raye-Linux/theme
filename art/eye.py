#!/usr/bin/env python3
#
# The Raye eye: a geometric almond in violet, cut by three scanline
# slices. Same drawing as the very first boot logo, now drawn on any
# background so it can also come out transparent.
#
# Rules I try to keep so it stays clean from 16px to 4K:
#   - one stroke weight for the outline, no lashes, no texture
#   - the glitch is three deliberate slices, not noise
#   - the only warm colour is the thin ring around the pupil
#
# Transparency is done by drawing the eye twice, once on black and once
# on white, and working the alpha out from the difference. That keeps
# the soft fade of the scanlines instead of a hard cut-out.

from PIL import Image, ImageChops, ImageDraw, ImageFont

W, H = 240, 240             # design grid, everything below is in it
S = 8                       # draw big, scale down, free antialiasing

VIOLET = (0x7b, 0x3c, 0xff)
MAGENTA = (0xd9, 0x46, 0xef)
DEEP = (0x2a, 0x10, 0x52)
LILAC = (0xb8, 0x8c, 0xff)
AMBER = (0xe0, 0xa4, 0x58)
INK = (0x12, 0x08, 0x1f)
WHITE = (0xf3, 0xea, 0xff)
NIGHT = (0x0e, 0x0a, 0x16)  # the background colour of the brand

CX, CY = 120, 100
A, B = 76, 34               # half width / half height of the eye
STROKE = 5
IRIS, PUPIL = 25, 10

# (top, height, shift) of the slices
SLICES = [(CY - 15, 4, 7), (CY + 5, 3, -5), (CY + 19, 2, 10)]


def box(x, y, r):
    return (int((x - r) * S), int((y - r) * S),
            int((x + r) * S), int((y + r) * S))


def almond(inset=0):
    """The eye as the overlap of two big circles (a vesica)."""
    r = (A * A + B * B) / (2 * B)
    m1 = Image.new("L", (W * S, H * S), 0)
    m2 = Image.new("L", (W * S, H * S), 0)
    ImageDraw.Draw(m1).ellipse(box(CX, CY - B + r, r - inset), fill=255)
    ImageDraw.Draw(m2).ellipse(box(CX, CY + B - r, r - inset), fill=255)
    return ImageChops.multiply(m1, m2)


def hgradient(a, b, x0, x1):
    img = Image.new("RGB", (W * S, H * S))
    d = ImageDraw.Draw(img)
    for x in range(W * S):
        t = min(1, max(0, (x / S - x0) / (x1 - x0)))
        d.line([(x, 0), (x, H * S)],
               fill=tuple(round(p + (q - p) * t) for p, q in zip(a, b)))
    return img


def radial(stops, r):
    """Concentric fill from the centre out; stops are (radius, colour)."""
    img = Image.new("RGB", (W * S, H * S), (0, 0, 0))
    d = ImageDraw.Draw(img)
    c = stops[-1][1]
    for i in range(int(r * S), 0, -1):
        x = i / S
        for (r0, c0), (r1, c1) in zip(stops, stops[1:]):
            if r0 <= x <= r1:
                t = (x - r0) / (r1 - r0)
                c = tuple(round(p + (q - p) * t) for p, q in zip(c0, c1))
                break
        d.ellipse(box(CX, CY, x), fill=c)
    return img


def mark(bg):
    img = Image.new("RGB", (W * S, H * S), bg)
    outer, inner = almond(), almond(STROKE)

    img.paste(INK, (0, 0), inner)

    iris = radial([(0, AMBER), (PUPIL + 1, AMBER), (PUPIL + 4, LILAC),
                   (IRIS - 6, VIOLET), (IRIS, DEEP)], IRIS)
    m = Image.new("L", img.size, 0)
    ImageDraw.Draw(m).ellipse(box(CX, CY, IRIS), fill=255)
    img.paste(iris, (0, 0), ImageChops.multiply(m, inner))

    d = ImageDraw.Draw(img)
    d.ellipse(box(CX, CY, PUPIL), fill=(0, 0, 0))
    d.ellipse(box(CX - 6, CY - 6, 3), fill=WHITE)
    d.ellipse(box(CX + 4, CY + 4, 1.2), fill=WHITE)

    rim = ImageChops.subtract(outer, inner)
    img.paste(hgradient(VIOLET, MAGENTA, CX - A, CX + A), (0, 0), rim)
    return img


def slice_up(img, bg, trails=True):
    out = img.copy()
    d = ImageDraw.Draw(out)
    for top, h, dx in SLICES:
        y0, y1 = top * S, (top + h) * S
        band = img.crop((0, y0, W * S, y1))
        out.paste(bg, (0, y0, W * S, y1))
        out.paste(band, (dx * S, y0))
        if not trails:
            continue
        # each slice leaves a scanline trailing off to the right,
        # fading as it goes
        start = CX + A - 6 + max(dx, 0)
        for i in range(48):
            t = i / 48
            c = tuple(round(b + (p - b) * (1 - t) ** 1.6) for p, b in
                      zip(MAGENTA if dx > 0 else LILAC, bg))
            x = start + 14 + i * 1.6
            d.rectangle((x * S, y0 + S, (x + 1.6) * S, y0 + 2 * S), fill=c)
    return out


def draw(bg, trails=True):
    return slice_up(mark(bg), bg, trails)


def matte(black, white):
    """RGBA from the same picture drawn on black and on white."""
    # on white the background adds (1 - alpha) * 255 to every channel
    alpha = ImageChops.invert(ImageChops.subtract(white, black).convert("L"))
    out = Image.merge("RGBA", (*black.split(), alpha))
    # un-premultiply the edge pixels so they don't go dark on light panels
    px = out.load()
    for y in range(out.height):
        for x in range(out.width):
            r, g, b, a = px[x, y]
            if 0 < a < 255:
                k = 255 / a
                px[x, y] = (min(255, round(r * k)), min(255, round(g * k)),
                            min(255, round(b * k)), a)
    return out


def transparent(trails=True):
    """The eye alone as RGBA."""
    return matte(draw((0, 0, 0), trails), draw((255, 255, 255), trails))


def crop_square(img, size, trails=True, pad=0.04):
    """Square crop around the eye, and its scanlines if they are kept."""
    left = CX - A - 2
    right = CX + A + (88 if trails else 12)
    side = (right - left) * (1 + 2 * pad)
    cx = (left + right) / 2
    sq = img.crop((round((cx - side / 2) * S), round((CY - side / 2) * S),
                   round((cx + side / 2) * S), round((CY + side / 2) * S)))
    return sq.resize((size, size), Image.LANCZOS)


def wordmark(img, font_path, colour=WHITE, size=19, y=168):
    d = ImageDraw.Draw(img)
    font = ImageFont.truetype(font_path, size * S)
    word, track = "RAYE", 11 * S
    widths = [d.textbbox((0, 0), ch, font=font)[2] for ch in word]
    total = sum(widths) + track * (len(word) - 1)
    x = (W * S - total) // 2
    for ch, w in zip(word, widths):
        d.text((x, y * S), ch, font=font, fill=colour)
        x += w + track
