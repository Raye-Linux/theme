#!/usr/bin/env python3
"""Checks a screenshot of run.sh: everything the boot screen draws (eye,
ring, words, password field) has to sit as one block in the middle of
every screen.

    check_block.py [--tolerance N] <shot.png> [x,y,w,h ...]

With no area the whole picture is one screen. run.sh passes the two
screens of the X11 renderer's fake multi-head setup (800x600 at 0,0 and
640x480 at 800,0, fixed in plymouth).

The block is found by its edges: the background and the light behind the
eye change slowly from one pixel to the next, the drawing does not. The
faint track of the ring counts, so the ring has the same size whichever
way the arc is turned. Exit status 1 when a screen fails."""

import sys

from PIL import Image

TOLERANCE = 2
# a step in brightness this big between two pixels is drawing
EDGE = 9


def block_box(img: Image.Image, area: tuple) -> tuple | None:
    x0, y0, w, h = area
    lum = img.convert("L").load()
    xs, ys = [], []
    # one pixel in from the edge of the screen: the next screen starts there
    for y in range(y0 + 2, y0 + h - 2):
        for x in range(x0 + 2, x0 + w - 2):
            v = lum[x, y]
            if abs(v - lum[x - 1, y]) >= EDGE or abs(v - lum[x, y - 1]) >= EDGE:
                xs.append(x)
                ys.append(y)
    if len(xs) < 30:
        return None
    # the edge of a shape is seen one pixel inside and one outside of it
    return min(xs) - 1, min(ys) - 1, max(xs), max(ys)


def check(path: str, areas: list, tolerance: float = TOLERANCE) -> list:
    img = Image.open(path).convert("RGB")
    if not areas:
        areas = [(0, 0, img.width, img.height)]
    errors = []
    for i, area in enumerate(areas):
        x0, y0, w, h = area
        box = block_box(img, area)
        if box is None:
            errors.append(f"{path}: screen {i}: nothing drawn")
            continue
        dx = (box[0] + box[2] + 1) / 2 - x0 - w / 2
        dy = (box[1] + box[3] + 1) / 2 - y0 - h / 2
        if abs(dx) > tolerance or abs(dy) > tolerance:
            errors.append(f"{path}: screen {i} ({w}x{h}): block {box[0] - x0},{box[1] - y0}"
                          f"-{box[2] - x0},{box[3] - y0} is {dx:+.1f},{dy:+.1f} off the middle")
    return errors


def offsets(path: str, areas: list) -> list:
    """(dx, dy) of the block on each screen, for a report."""
    img = Image.open(path).convert("RGB")
    out = []
    for x0, y0, w, h in areas or [(0, 0, img.width, img.height)]:
        box = block_box(img, (x0, y0, w, h))
        if box is None:
            out.append(None)
        else:
            out.append(((box[0] + box[2] + 1) / 2 - x0 - w / 2, (box[1] + box[3] + 1) / 2 - y0 - h / 2))
    return out


def main(argv: list) -> int:
    tolerance = TOLERANCE
    if argv[:1] == ["--tolerance"]:
        tolerance = float(argv[1])
        argv = argv[2:]
    if not argv:
        print(__doc__, file=sys.stderr)
        return 2
    areas = [tuple(int(v) for v in a.split(",")) for a in argv[1:]]
    errors = check(argv[0], areas, tolerance)
    for e in errors:
        print("FAIL " + e)
    if not errors:
        print("ok   " + argv[0] + "  " + "  ".join(
            f"{d[0]:+.1f},{d[1]:+.1f}" for d in offsets(argv[0], areas) if d))
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
