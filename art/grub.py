#!/usr/bin/env python3
#
# Pictures for the GRUB menu of an installed Raye (grub/raye/):
#
#   background.png     the night background
#   logo.png           the eye at rest, centred in its square
#   select_*.png       the rounded highlight behind the selected entry,
#                      cut in three so GRUB can stretch the middle
#   hint.png           the keys that drive the menu: ↑ ↓ and Enter
#   timer_*.png        the countdown bar, empty and filled
#   icons/raye.png     the eye, beside every Raye entry (--class raye)
#   icons/os.png       a screen, beside any other system (--class os)
#
# Under Secure Boot the signed GRUB refuses every font read from disk,
# so whatever must look like Raye is a picture. The entry titles can't
# be: they come from update-grub (kernel versions, the disk Windows is
# on), so GRUB draws them, in Inter when it may load it and in its own
# Unifont when it can't. The live image draws its menu with the same
# rules (iso/boot/grub/art.py) and takes background, logo and select_*
# from here.
#
#   python3 art/grub.py

import os
import sys

from PIL import Image, ImageDraw, ImageEnhance, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import eye  # noqa: E402
import render  # noqa: E402

OUT = os.path.join(render.ROOT, "grub", "raye")
INTER = "/usr/share/fonts/opentype/inter"

ROW = 44           # height of one menu entry, must match theme.txt
ICON = 32          # icon_width and icon_height in theme.txt
LOGO = 220
TIMER_W, TIMER_H = 200, 28   # GRUB makes a progress bar a text line high

TEXT = (243, 234, 255)
SECONDARY = (185, 168, 214)
QUIET = (141, 126, 176)
ACCENT = (123, 60, 255)

S = 4              # draw big, scale down


def shrink(img: Image.Image, size: tuple) -> Image.Image:
    return img.resize(size, Image.LANCZOS)


def highlight() -> None:
    """Rounded violet pill, drawn big then split into west/centre/east."""
    w, h, r = 3 * ROW * S, ROW * S, ROW * S // 2 - 2 * S
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    ImageDraw.Draw(img).rounded_rectangle(
        (0, 0, w - 1, h - 1), radius=r, fill=(123, 60, 255, 150),
        outline=(184, 140, 255, 220), width=S)
    img = shrink(img, (3 * ROW, ROW))
    edge = ROW // 2
    img.crop((0, 0, edge, ROW)).save(os.path.join(OUT, "select_w.png"))
    img.crop((edge, 0, edge + 4, ROW)).save(os.path.join(OUT, "select_c.png"))
    img.crop((3 * ROW - edge, 0, 3 * ROW, ROW)).save(os.path.join(OUT, "select_e.png"))


def whole_eye(width: float, size: int) -> Image.Image:
    """The eye without its scanline slices (as on the boot screen), the
    almond `width` of a `size` square."""
    img = eye.matte(eye.mark((0, 0, 0)), eye.mark((255, 255, 255)))
    side = 2 * (eye.A + eye.STROKE) / width
    box = (round((eye.CX - side / 2) * eye.S), round((eye.CY - side / 2) * eye.S),
           round((eye.CX + side / 2) * eye.S), round((eye.CY + side / 2) * eye.S))
    return shrink(img.crop(box), (size, size))


def screen_icon() -> Image.Image:
    """Outline of a display on its foot: any system that isn't Raye."""
    u = S * ICON / 28     # drawn on a 28 grid
    img = Image.new("RGBA", (ICON * S, ICON * S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    c = SECONDARY + (255,)
    d.rounded_rectangle((3 * u, 5 * u, 25 * u, 20 * u), radius=3 * u,
                        outline=c, width=round(2.2 * u))
    d.rectangle((13 * u, 20 * u, 15 * u, 23 * u), fill=c)
    d.rounded_rectangle((9 * u, 23 * u, 19 * u, 25 * u), radius=u, fill=c)
    return shrink(img, (ICON, ICON))


def key(label: str, width: int, f: ImageFont.FreeTypeFont) -> Image.Image:
    """One key cap, drawn at S."""
    h = 26 * S
    img = Image.new("RGBA", (width * S, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((0, 0, width * S - 1, h - 1), radius=6 * S,
                        fill=TEXT + (18,), outline=QUIET + (150,), width=round(1.25 * S))
    d.text((width * S // 2, h // 2), label, font=f, fill=SECONDARY + (255,), anchor="mm")
    return img


def hint() -> Image.Image:
    """↑ ↓ and the Enter key, no words: the menu shows in any language."""
    f = ImageFont.truetype(os.path.join(INTER, "Inter-Regular.otf"), 15 * S)
    caps = [key("↑", 30, f), key("↓", 30, f), key("↵", 46, f)]
    gaps = [6, 18]
    w = sum(c.width for c in caps) + sum(gaps) * S
    img = Image.new("RGBA", (w, caps[0].height), (0, 0, 0, 0))
    x = 0
    for c, g in zip(caps, gaps + [0]):
        img.alpha_composite(c, (x, 0))
        x += c.width + g * S
    return shrink(img, (w // S, img.height // S))


def stripe(colour: tuple, alpha: int) -> Image.Image:
    img = Image.new("RGBA", (2, TIMER_H), (0, 0, 0, 0))
    ImageDraw.Draw(img).rectangle((0, TIMER_H // 2 - 2, 1, TIMER_H // 2 + 1),
                                  fill=colour + (alpha,))
    return img


def main() -> None:
    os.makedirs(os.path.join(OUT, "icons"), exist_ok=True)
    bg = render.wallpaper(1920, 1080)
    ImageEnhance.Brightness(bg).enhance(0.55).save(
        os.path.join(OUT, "background.png"), optimize=True)
    whole_eye(0.62, LOGO).save(os.path.join(OUT, "logo.png"), optimize=True)
    highlight()
    hint().save(os.path.join(OUT, "hint.png"), optimize=True)
    stripe(TEXT, 36).save(os.path.join(OUT, "timer_c.png"))
    stripe(ACCENT, 255).save(os.path.join(OUT, "timer_fill_c.png"))
    whole_eye(1.0, ICON).save(os.path.join(OUT, "icons", "raye.png"), optimize=True)
    screen_icon().save(os.path.join(OUT, "icons", "os.png"), optimize=True)


if __name__ == "__main__":
    main()
