#!/usr/bin/env python3
#
# The eye as half-block text, for fastfetch and anything else that runs
# in a terminal. Uses the same shapes as eye.py, sampled down to one
# character per 1x2 pixels. $1 is the lid, $2 the iris (fastfetch fills
# in the colours).
#
#   python3 art/ascii.py > fetch/raye.txt

import os
import sys

from PIL import Image, ImageChops, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import eye as logo  # noqa: E402

COLS, ROWS = 38, 9
S = logo.S

lid = ImageChops.subtract(logo.almond(), logo.almond(logo.STROKE + 2))
iris = Image.new("L", lid.size, 0)
d = ImageDraw.Draw(iris)
d.ellipse(logo.box(logo.CX, logo.CY, logo.IRIS), fill=255)
d.ellipse(logo.box(logo.CX, logo.CY, logo.PUPIL + 1), fill=0)

# just the eye, with a bit of air around it
x0, x1 = logo.CX - logo.A - 2, logo.CX + logo.A + 2
h = (x1 - x0) * ROWS * 2 / COLS / 2
y0, y1 = logo.CY - h, logo.CY + h
box = tuple(int(v * S) for v in (x0, y0, x1, y1))
size = (COLS, ROWS * 2)
lid = lid.crop(box).resize(size, Image.BOX).load()
iris = iris.crop(box).resize(size, Image.BOX).load()


def cls(x, y):
    if iris[x, y] > 110:
        return 2
    if lid[x, y] > 70:
        return 1
    return 0


for row in range(ROWS):
    line, colour = "", 0
    for x in range(COLS):
        top, bot = cls(x, row * 2), cls(x, row * 2 + 1)
        c = max(top, bot)
        if c == 0:
            line += " "
            continue
        if c != colour:
            line += f"${c}"
            colour = c
        if top and bot:
            line += "█"
        elif top:
            line += "▀"
        else:
            line += "▄"
    print(line.rstrip())
