#!/usr/bin/env python3
#
# Pictures of the sign-in surfaces (login screen, lock screen, splash,
# security screen), all in assets/signin/:
#
#   face-256.png, face-512.png   the default account picture: a Raye
#                                silhouette on the night gradient, lit by
#                                the eye's violet glow. Square; the screens
#                                cut it round.
#   raye-night-blur-1920.jpg,    the default wallpaper blurred (Gaussian,
#   raye-night-blur-3840.jpg     sigma 32 px at 1080p) with the sign-in
#                                tint (#0e0a17 at 62 %) already on it.
#                                For screens that cannot blur (software
#                                rendering): draw it as it is, no tint
#                                on top.
#   first-login-marker.png       1x1 picture; its presence in the user's
#                                state folder tells the splash it is the
#                                account's first sign-in.
#   icons/*.svg                  the corner bar and flyout icons, 20 px
#                                grid, drawn in the sign-in text colour.
#
#   python3 art/signin.py           write them
#   python3 art/signin.py --check   say which files are missing or out of
#                                   date, exit 1 if there are any
#
# The output is committed, so the package build doesn't need Pillow.
# Run it again when a drawing here or the default wallpaper changes.

import io
import math
import os
import random
import sys
import tempfile

from PIL import Image, ImageChops, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "assets", "signin")
WALLPAPER = os.path.join(ROOT, "wallpaper", "Raye", "contents", "images",
                         "3840x2160.png")

# the sign-in palette (lock-login spec, section 3.0)
NIGHT = (0x10, 0x0f, 0x13)
DEEP = (0x2b, 0x25, 0x3a)
VIOLET = (0x7b, 0x3c, 0xff)
GLOW = (0x7b, 0x3c, 0xff)
TEXT = (0xf1, 0xf0, 0xf4)
TEXT2 = (0xb5, 0xb3, 0xbc)
ERROR = "#ff7a9a"
ICON = "#f1f0f4"

SIGNIN_TINT = 0.62          # tint over the picture on the sign-in view
BLUR_SIGMA_1080 = 32        # Gaussian sigma at 1080 px of height
BLUR_SIZES = (1920, 3840)
FACE_SIZES = (256, 512)
BATTERY_LEVELS = tuple(range(0, 101, 10))
WIFI_LEVELS = (0, 1, 2, 3, 4)
JPEG_QUALITY = 90

# icons: 20 x 20 grid, one stroke weight, round ends
GRID = 20
STROKE = 1.5
DIM = 0.3                   # opacity of the parts of a meter that are off


# --- pictures ---------------------------------------------------------

def blur_sigma(height):
    """Blur strength for a picture of this height, same look at any size."""
    return BLUR_SIGMA_1080 * height / 1080


def dither(size, seed=7):
    """Fixed fine noise, -2..+2 per channel, so the soft gradients don't
    band once they are saved as JPEG. Same noise on every run."""
    tile = 256
    rnd = random.Random(seed)
    planes = []
    for _ in range(3):
        raw = Image.frombytes("L", (tile, tile), rnd.randbytes(tile * tile))
        # two uniform 0..2 values added: a triangle around 2
        planes.append(raw.point(lambda v: v % 3 + (v // 3) % 3))
    noise = Image.merge("RGB", planes)
    out = Image.new("RGB", size)
    for y in range(0, size[1], tile):
        for x in range(0, size[0], tile):
            out.paste(noise, (x, y))
    return out


def blurred_wallpaper(source, width):
    """The wallpaper at this width (16:9), blurred and tinted."""
    height = width * 9 // 16
    img = source.convert("RGB").resize((width, height), Image.LANCZOS)
    # blur at a quarter of the size: same result on a picture this soft,
    # sixteen times less work
    small = img.resize((width // 4, height // 4), Image.LANCZOS)
    small = small.filter(ImageFilter.GaussianBlur(blur_sigma(height) / 4))
    img = small.resize((width, height), Image.BICUBIC)
    over = Image.new("RGB", img.size, NIGHT)
    img = Image.blend(img, over, SIGNIN_TINT)
    return ImageChops.add(img, dither(img.size), 1.0, -2)


def radial(size, centre, radius, inner, outer):
    """Colour going from inner at centre to outer at radius and beyond."""
    ramp = Image.radial_gradient("L")      # 256 x 256, 0 in the middle
    side = round(radius * 2 * 256 / 255)
    ramp = ramp.resize((side, side), Image.BICUBIC)
    mask = Image.new("L", size, 255)
    mask.paste(ramp, (round(centre[0] - side / 2), round(centre[1] - side / 2)))
    return Image.composite(Image.new("RGB", size, outer),
                           Image.new("RGB", size, inner), mask)


def face(size):
    """Default account picture: silhouette, glow, night gradient."""
    k = 4                               # draw big, scale down
    n = size * k
    img = radial((n, n), (n * 0.5, n * 0.42), n * 0.78, DEEP, NIGHT)

    # the eye's glow behind the head and shoulders
    glow = Image.new("RGB", (n, n), (0, 0, 0))
    d = ImageDraw.Draw(glow)
    d.ellipse((n * 0.12, n * 0.18, n * 0.88, n * 1.05), fill=(60, 26, 128))
    d.ellipse((n * 0.30, n * 0.12, n * 0.70, n * 0.56), fill=(88, 30, 120))
    glow = glow.filter(ImageFilter.GaussianBlur(n * 0.11))
    img = ImageChops.add(img, glow)

    # head and shoulders, apart, the way account pictures are drawn
    shape = Image.new("L", (n, n), 0)
    d = ImageDraw.Draw(shape)
    head_r = n * 0.15
    head = (n * 0.5, n * 0.38)
    d.ellipse((head[0] - head_r, head[1] - head_r,
               head[0] + head_r, head[1] + head_r), fill=255)
    gap = n * 0.035
    top = head[1] + head_r + gap
    d.ellipse((n * 0.13, top, n * 0.87, top + n * 0.95), fill=255)
    shape = shape.filter(ImageFilter.GaussianBlur(k * 0.4))

    # light from above: lilac white at the top, the secondary text colour
    # lower down, a rim of glow colour where it meets the background
    body = Image.new("RGB", (n, n))
    d = ImageDraw.Draw(body)
    for y in range(n):
        t = min(1.0, max(0.0, (y / n - 0.25) / 0.7))
        d.line([(0, y), (n, y)],
               fill=tuple(round(a + (b - a) * t) for a, b in zip(TEXT, TEXT2)))
    rim = ImageChops.subtract(shape, shape.filter(ImageFilter.MinFilter(k * 2 + 1)))
    lit = Image.composite(Image.new("RGB", (n, n), GLOW), body, rim.point(lambda v: v // 3))
    img = Image.composite(lit, img, shape.point(lambda v: round(v * 0.88)))
    return img.resize((size, size), Image.LANCZOS)


def marker():
    return Image.new("RGBA", (1, 1), VIOLET + (255,))


# --- icons ------------------------------------------------------------

def f(v):
    """A number the way the SVG files write it."""
    s = f"{v:.2f}".rstrip("0").rstrip(".")
    return "0" if s in ("-0", "") else s


def pt(x, y):
    return f"{f(x)} {f(y)}"


def on_circle(cx, cy, r, deg):
    """Point of a circle; 0 degrees is straight up, clockwise."""
    a = math.radians(deg)
    return cx + r * math.sin(a), cy - r * math.cos(a)


def stroke(d, opacity=1.0, colour=ICON, width=STROKE):
    extra = f' stroke-opacity="{f(opacity)}"' if opacity < 1 else ""
    return (f'<path d="{d}" fill="none" stroke="{colour}" '
            f'stroke-width="{f(width)}" stroke-linecap="round" '
            f'stroke-linejoin="round"{extra}/>')


def fill(d, opacity=1.0, colour=ICON):
    extra = f' fill-opacity="{f(opacity)}"' if opacity < 1 else ""
    return f'<path d="{d}" fill="{colour}"{extra}/>'


def circle_path(cx, cy, r):
    return (f"M{pt(cx - r, cy)} A{f(r)} {f(r)} 0 1 0 {pt(cx + r, cy)} "
            f"A{f(r)} {f(r)} 0 1 0 {pt(cx - r, cy)} Z")


def rect_path(x, y, w, h, r):
    r = min(r, w / 2, h / 2)
    return (f"M{pt(x + r, y)} H{f(x + w - r)} A{f(r)} {f(r)} 0 0 1 {pt(x + w, y + r)} "
            f"V{f(y + h - r)} A{f(r)} {f(r)} 0 0 1 {pt(x + w - r, y + h)} "
            f"H{f(x + r)} A{f(r)} {f(r)} 0 0 1 {pt(x, y + h - r)} "
            f"V{f(y + r)} A{f(r)} {f(r)} 0 0 1 {pt(x + r, y)} Z")


def svg(*parts):
    body = "\n  ".join(parts)
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{GRID}" '
            f'height="{GRID}" viewBox="0 0 {GRID} {GRID}">\n  {body}\n</svg>\n')


def icon_power():
    cx, cy, r = 10, 10.75, 6.5
    a = on_circle(cx, cy, r, -40)
    b = on_circle(cx, cy, r, 40)
    return svg(stroke(f"M{pt(*a)} A{f(r)} {f(r)} 0 1 0 {pt(*b)}"),
               stroke("M10 2.75 V9.75"))


def icon_sleep():
    # a crescent: the big circle less a circle moved up and right
    cx, cy, r = 9.5, 10.5, 7
    ox, oy, orr = 13.5, 6.5, 5.5
    dx, dy = ox - cx, oy - cy
    dist = math.hypot(dx, dy)
    a = (r * r - orr * orr + dist * dist) / (2 * dist)
    h = math.sqrt(r * r - a * a)
    mx, my = cx + a * dx / dist, cy + a * dy / dist
    p1 = (mx - h * dy / dist, my + h * dx / dist)
    p2 = (mx + h * dy / dist, my - h * dx / dist)
    return svg(stroke(f"M{pt(*p2)} A{f(r)} {f(r)} 0 1 0 {pt(*p1)} "
                      f"A{f(orr)} {f(orr)} 0 0 1 {pt(*p2)} Z"))


def icon_restart():
    # a circle open at the top right, the arrow at its end going clockwise
    cx, cy, r = 10, 10.5, 6.5
    start = on_circle(cx, cy, r, 45)
    tip = on_circle(cx, cy, r, 5)
    a = math.radians(5)
    along = (math.cos(a), math.sin(a))      # clockwise direction there
    out = (math.sin(a), -math.cos(a))       # away from the centre
    size = 3.25
    head_a = (tip[0] - size * along[0] + size * out[0],
              tip[1] - size * along[1] + size * out[1])
    head_b = (tip[0] - size * along[0] - size * out[0],
              tip[1] - size * along[1] - size * out[1])
    return svg(stroke(f"M{pt(*start)} A{f(r)} {f(r)} 0 1 1 {pt(*tip)}"),
               stroke(f"M{pt(*head_a)} L{pt(*tip)} L{pt(*head_b)}"))


def icon_accessibility():
    return svg(fill(circle_path(10, 3.9, 1.75)),
               stroke("M3.5 7.25 Q10 8.75 16.5 7.25"),
               stroke("M10 8.25 V12.25 L6.75 17.25 M10 12.25 L13.25 17.25"))


def icon_keyboard():
    parts = [stroke(rect_path(2, 4.75, 16, 10.5, 2))]
    for row, y in enumerate((7.75, 10.25)):
        for x in (5, 7.75, 10.5, 13.25) if row == 0 else (6.25, 9, 11.75, 14.5):
            parts.append(fill(circle_path(x + 0.5, y, 0.8)))
    parts.append(stroke("M7 12.75 H13"))
    return svg(*parts)


def letter_a(left, right, top, bottom, bar):
    """Outline of a capital A: two legs and the bar at height bar."""
    mid = (left + right) / 2
    t = (bar - top) / (bottom - top)
    return (f"M{pt(left, bottom)} L{pt(mid, top)} L{pt(right, bottom)} "
            f"M{pt(mid + (left - mid) * t, bar)} H{f(mid + (right - mid) * t)}")


def icon_text_larger():
    return svg(stroke(letter_a(1.75, 11.75, 3.5, 16.25, 12)),
               stroke(letter_a(12.75, 18.25, 9.25, 16.25, 14)))


def icon_contrast():
    return svg(stroke(circle_path(10, 10, 7)),
               fill("M10 3 A7 7 0 0 1 10 17 Z"))


def icon_network_wired():
    return svg(stroke(rect_path(2.25, 3, 15.5, 10.5, 1.75)),
               stroke("M10 13.5 V16.5 M6.5 17 H13.5"))


WIFI_DOT = (10, 15.75)
WIFI_RADII = (4, 7.25, 10.5)
WIFI_SPAN = 45                      # degrees each side of straight up


def wifi_parts(level):
    """The dot and three arcs; the first `level` of them are lit."""
    parts = [fill(circle_path(*WIFI_DOT, 1.35), 1.0 if level >= 1 else DIM)]
    for i, r in enumerate(WIFI_RADII):
        a = on_circle(*WIFI_DOT, r, -WIFI_SPAN)
        b = on_circle(*WIFI_DOT, r, WIFI_SPAN)
        parts.append(stroke(f"M{pt(*a)} A{f(r)} {f(r)} 0 0 1 {pt(*b)}",
                            1.0 if level >= i + 2 else DIM))
    return parts


def icon_wifi(level):
    return svg(*wifi_parts(level))


def icon_network_none():
    # a globe and a small "no" sign: there is no connection
    cx, cy, r = 8.75, 8.75, 6
    globe = [stroke(circle_path(cx, cy, r)),
             stroke(f"M{pt(cx, cy - r)} A2.75 {f(r)} 0 0 0 {pt(cx, cy + r)} "
                    f"A2.75 {f(r)} 0 0 0 {pt(cx, cy - r)}"),
             stroke(f"M{pt(cx - r, cy)} H{f(cx + r)}")]
    bx, by, br = 15.25, 15.25, 3.25
    s = br * 0.62
    no = [stroke(circle_path(bx, by, br), width=1.25),
          stroke(f"M{pt(bx - s, by + s)} L{pt(bx + s, by - s)}", width=1.25)]
    return svg(*globe, *no)


def icon_battery(level, charging=False):
    """Battery seen from the side, filled to `level` percent. Charging
    puts a bolt next to it; at 10 % or less on battery it turns red."""
    low = level <= 10 and not charging
    colour = ERROR if low else ICON
    # charging: a shorter body, so the bolt stands clear of the terminal
    w = 10.5 if charging else 14
    x, y, h = 1.75, 6, 8
    parts = [stroke(rect_path(x, y, w, h, 2), colour=colour),
             fill(rect_path(x + w + 0.5, 8.25, 1.75, 3.5, 0.75), colour=colour)]
    inner = w - 3
    if level > 0:
        parts.append(fill(rect_path(x + 1.5, y + 1.5, max(1.0, inner * level / 100),
                                    h - 3, 1), colour=colour))
    if charging:
        parts.append(fill("M17.75 4.5 L15.25 10.5 H17.25 L16.5 15.5 L19.25 9 H17.25 Z"))
    return svg(*parts)


def eye_outline():
    return "M1.75 10 Q10 2 18.25 10 Q10 18 1.75 10 Z"


def icon_eye_reveal():
    return svg(stroke(eye_outline()), fill(circle_path(10, 10, 2.5)))


def icon_eye_hide():
    return svg(stroke(eye_outline()), fill(circle_path(10, 10, 2.5)),
               stroke("M3.5 3.5 L16.5 16.5"))


def icon_arrow():
    return svg(stroke("M3.75 10 H16 M11 5 L16 10 L11 15"))


def icon_lock():
    return svg(stroke(rect_path(4, 9, 12, 8.25, 1.75)),
               stroke("M6.75 9 V6.5 A3.25 3.25 0 0 1 13.25 6.5 V9"),
               fill(circle_path(10, 13.1, 1.2)))


def icon_switch_user():
    return svg(stroke(circle_path(7.5, 6.5, 3)),
               stroke("M2.25 17 A5.25 5.25 0 0 1 12.75 17"),
               stroke("M14 5.25 H18 M16.25 3.25 L18.25 5.25 L16.25 7.25"),
               stroke("M18 10.25 H14 M15.75 8.25 L13.75 10.25 L15.75 12.25"))


def icon_sign_out():
    return svg(stroke("M8.25 3 H5 A1.75 1.75 0 0 0 3.25 4.75 V15.25 A1.75 1.75 0 0 0 5 17 H8.25"),
               stroke("M8 10 H17 M13.25 6.25 L17 10 L13.25 13.75"))


def icon_key():
    return svg(stroke(circle_path(6.5, 13.5, 3.75)),
               stroke("M9.25 10.75 L16.75 3.25 M13.5 6.5 L15.75 8.75 M15.75 4.25 L17.25 5.75"))


def icon_task_manager():
    return svg(stroke(rect_path(2.25, 3.25, 15.5, 13.5, 1.75)),
               stroke("M5 10.5 H7.25 L8.75 7 L11 13 L12.5 10.5 H15"))


def icon_desktop():
    # two windows, the one in front with its title bar
    return svg(stroke("M5.75 6 V4.75 A1.75 1.75 0 0 1 7.5 3 H16.25 A1.75 1.75 0 0 1 18 4.75 "
                      "V11.25 A1.75 1.75 0 0 1 16.25 13 H15"),
               stroke(rect_path(2, 7, 13, 10, 1.75)),
               stroke("M2 10 H15"))


def icon_check():
    return svg(stroke("M4.25 10.5 L8.25 14.25 L15.75 6"))


def icons():
    """File name in icons/ -> SVG text."""
    out = {
        "power.svg": icon_power(),
        "sleep.svg": icon_sleep(),
        "restart.svg": icon_restart(),
        "accessibility.svg": icon_accessibility(),
        "keyboard.svg": icon_keyboard(),
        "text-larger.svg": icon_text_larger(),
        "contrast.svg": icon_contrast(),
        "network-wired.svg": icon_network_wired(),
        "network-none.svg": icon_network_none(),
        "eye-reveal.svg": icon_eye_reveal(),
        "eye-hide.svg": icon_eye_hide(),
        "arrow-right.svg": icon_arrow(),
        "lock.svg": icon_lock(),
        "switch-user.svg": icon_switch_user(),
        "sign-out.svg": icon_sign_out(),
        "key.svg": icon_key(),
        "task-manager.svg": icon_task_manager(),
        "desktop.svg": icon_desktop(),
        "check.svg": icon_check(),
    }
    for n in WIFI_LEVELS:
        out[f"network-wireless-{n}.svg"] = icon_wifi(n)
    for n in BATTERY_LEVELS:
        out[f"battery-{n:03d}.svg"] = icon_battery(n)
        out[f"battery-{n:03d}-charging.svg"] = icon_battery(n, True)
    return out


# --- writing and checking ---------------------------------------------

def pictures():
    """File name -> (PIL image, format)."""
    source = Image.open(WALLPAPER)
    out = {f"face-{n}.png": (face(n), "PNG") for n in FACE_SIZES}
    for w in BLUR_SIZES:
        out[f"raye-night-blur-{w}.jpg"] = (blurred_wallpaper(source, w), "JPEG")
    out["first-login-marker.png"] = (marker(), "PNG")
    return out


def save(img, fmt, path):
    if fmt == "JPEG":
        img.save(path, "JPEG", quality=JPEG_QUALITY, subsampling=0,
                 optimize=True)
    else:
        img.save(path, "PNG", optimize=True)


def write(out=OUT):
    os.makedirs(os.path.join(out, "icons"), exist_ok=True)
    for name, (img, fmt) in pictures().items():
        save(img, fmt, os.path.join(out, name))
    for name, text in icons().items():
        with open(os.path.join(out, "icons", name), "w", encoding="utf-8") as fh:
            fh.write(text)


def same_picture(path, img, fmt):
    """True when the file holds this picture. JPEG is compared after a
    round trip through the same encoder, so it is exact too."""
    try:
        have = Image.open(path)
        have.load()
    except (OSError, ValueError):
        return False
    if fmt == "JPEG":
        buf = io.BytesIO()
        save(img, fmt, buf)
        img = Image.open(io.BytesIO(buf.getvalue()))
    if have.size != img.size or have.mode != img.mode:
        return False
    return ImageChops.difference(have, img).getbbox() is None


def stale(out=OUT):
    """Files that are missing or differ from what this script draws, and
    files in the folder this script does not draw."""
    bad = []
    want = set()
    for name, (img, fmt) in pictures().items():
        want.add(name)
        if not same_picture(os.path.join(out, name), img, fmt):
            bad.append(name)
    for name, text in icons().items():
        want.add(os.path.join("icons", name))
        try:
            with open(os.path.join(out, "icons", name), encoding="utf-8") as fh:
                if fh.read() == text:
                    continue
        except OSError:
            pass
        bad.append(os.path.join("icons", name))
    if os.path.isdir(out):
        for base, _dirs, files in os.walk(out):
            for name in files:
                rel = os.path.relpath(os.path.join(base, name), out)
                if rel not in want:
                    bad.append(rel + " (not drawn by art/signin.py)")
    return sorted(bad)


def main(argv):
    if argv[1:] == ["--check"]:
        bad = stale()
        for name in bad:
            print(f"out of date: assets/signin/{name}")
        return 1 if bad else 0
    if argv[1:]:
        print("usage: art/signin.py [--check]", file=sys.stderr)
        return 2
    # write next to the target first, so a failure halfway never leaves
    # a half-written set behind
    with tempfile.TemporaryDirectory(dir=os.path.dirname(OUT)) as tmp:
        write(tmp)
        os.makedirs(OUT, exist_ok=True)
        for base, _dirs, files in os.walk(tmp):
            for name in files:
                src = os.path.join(base, name)
                dst = os.path.join(OUT, os.path.relpath(src, tmp))
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                os.replace(src, dst)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
