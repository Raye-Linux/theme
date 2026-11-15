#!/usr/bin/env python3
#
# Renders every picture the theme ships, from eye.py:
#
#   assets/icons/raye-eye-<n>.png      the eye alone, transparent, icon sizes
#   assets/logo/raye-eye.png           eye with its scanlines, transparent
#   assets/logo/raye-logo.png          eye + RAYE wordmark, transparent
#   assets/logo/raye-logo-night.png    same on the night background
#   assets/wallpapers/raye-night*.png  default wallpaper
#   assets/plymouth/                   boot screen pictures, with the
#                                      disk password field and its text
#
#   python3 art/render.py [font]
#
# The output is committed, so the package build doesn't need Pillow.
# Run it again only when the drawing changes.

import os
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import eye  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "assets")
FONT = "/usr/share/fonts/opentype/inter/Inter-SemiBold.otf"
TEXT_FONT = "/usr/share/fonts/opentype/inter/Inter-Regular.otf"

# what the boot screen says when the disk wants its password; the
# initramfs hook picks the language of the system
PROMPTS = {
    "en": ("Enter the password of this disk",
           "Your recovery key works here too"),
    "fr": ("Entrez le mot de passe du disque",
           "Votre clé de récupération fonctionne aussi"),
}
ICON_SIZES = (16, 22, 24, 32, 48, 64, 96, 128, 256, 512)


def out(*parts: str) -> str:
    path = os.path.join(OUT, *parts)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    return path


def logo_with_wordmark(font: str, bg: tuple[int, int, int] | None) -> Image.Image:
    """Eye and wordmark, cropped to the drawing. bg=None means transparent."""
    if bg is None:
        black = eye.draw((0, 0, 0))
        white = eye.draw((255, 255, 255))
        eye.wordmark(black, font)
        eye.wordmark(white, font)
        img = eye.matte(black, white)
    else:
        img = eye.draw(bg)
        eye.wordmark(img, font)
    s = eye.S
    return img.crop((20 * s, 40 * s, 220 * s, 210 * s))


def wallpaper(w: int = 3840, h: int = 2160) -> Image.Image:
    """Night background with soft violet glows. Drawn small, blurred and
    scaled up, so the gradients stay smooth and the file stays small."""
    sw, sh = w // 10, h // 10
    base = Image.new("RGB", (sw, sh), eye.NIGHT)
    glow = Image.new("RGB", (sw, sh), (0, 0, 0))
    d = ImageDraw.Draw(glow)
    d.ellipse((-sw * 0.15, sh * 0.35, sw * 0.55, sh * 1.35), fill=(70, 30, 150))
    d.ellipse((sw * 0.55, -sh * 0.45, sw * 1.2, sh * 0.55), fill=(120, 40, 140))
    d.ellipse((sw * 0.30, sh * 0.30, sw * 0.70, sh * 0.80), fill=(40, 18, 80))
    glow = glow.filter(ImageFilter.GaussianBlur(sw * 0.12))
    return ImageChops.add(base, glow).resize((w, h), Image.BICUBIC)


def main() -> None:
    font = sys.argv[1] if len(sys.argv) > 1 else FONT

    icon = eye.crop_square(eye.transparent(trails=False), 1024, trails=False)
    for n in ICON_SIZES:
        icon.resize((n, n), Image.LANCZOS).save(
            out("icons", f"raye-eye-{n}.png"), optimize=True)

    with_trails = eye.transparent(trails=True)
    eye.crop_square(with_trails, 1024).save(out("logo", "raye-eye.png"),
                                           optimize=True)
    eye.crop_square(with_trails, 320).save(out("plymouth", "eye.png"),
                                          optimize=True)

    logo_with_wordmark(font, None).resize((1000, 850), Image.LANCZOS) \
        .save(out("logo", "raye-logo.png"), optimize=True)
    logo_with_wordmark(font, eye.NIGHT).resize((1000, 850), Image.LANCZOS) \
        .save(out("logo", "raye-logo-night.png"), optimize=True)

    wallpaper().save(out("wallpapers", "raye-night.png"), optimize=True)
    wallpaper(1920, 1080).save(out("wallpapers", "raye-night-1080.png"),
                               optimize=True)

    # one dot for the boot spinner, drawn big for clean edges
    dot = Image.new("RGBA", (48, 48), (0, 0, 0, 0))
    ImageDraw.Draw(dot).ellipse((0, 0, 47, 47), fill=eye.LILAC + (255,))
    dot.resize((12, 12), Image.LANCZOS).save(out("plymouth", "dot.png"))

    password_field(font)


def text_line(text: str, font: str, size: int, colour: tuple) -> Image.Image:
    """One line of text on a transparent background, tight around it."""
    f = ImageFont.truetype(font, size * 4)
    box = ImageDraw.Draw(Image.new("L", (1, 1))).textbbox((0, 0), text, font=f)
    w, h = box[2] - box[0] + 8, box[3] - box[1] + 8
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    ImageDraw.Draw(img).text((4 - box[0], 4 - box[1]), text, font=f,
                             fill=colour + (255,))
    return img.resize((w // 4, h // 4), Image.LANCZOS)


def password_field(font: str) -> None:
    """The rounded field the password dots go in, and its two lines of
    text per language. Drawn here because the initramfs has no Inter."""
    w, h, k = 380, 50, 4
    field = Image.new("RGBA", (w * k, h * k), (0, 0, 0, 0))
    ImageDraw.Draw(field).rounded_rectangle(
        (k, k, w * k - k - 1, h * k - k - 1), radius=14 * k,
        fill=(0x17, 0x11, 0x1f, 240), outline=eye.VIOLET + (255,), width=2 * k)
    field.resize((w, h), Image.LANCZOS).save(out("plymouth", "field.png"))

    for lang, (title, hint) in PROMPTS.items():
        title_img = text_line(title, font, 20, eye.WHITE)
        hint_img = text_line(hint, TEXT_FONT, 14, (0x9a, 0x8f, 0xae))
        title_img.save(out("plymouth", f"prompt-{lang}.png"))
        hint_img.save(out("plymouth", f"hint-{lang}.png"))


if __name__ == "__main__":
    main()
