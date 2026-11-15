#!/usr/bin/env python3
#
# The task buttons of the Raye Plasma theme (widgets/tasks.svg), for the
# stock task manager. Raye's own taskbar draws its buttons itself; this
# file is what shows when that taskbar cannot load and Plasma falls back
# to its icon task manager. The taskbar background and every other
# surface of the Plasma style come from art/plasmastyle.py.
#
# Plasma cuts a frame into nine named pieces (corners, edges, centre),
# draws each piece on its own and stretches the edges and the centre.
# So every piece is a group holding its own slice of the drawing, plus an
# invisible rect that gives the piece its full size. Nothing may be drawn
# outside that rect: Plasma measures a piece by everything in it.
#
# The task indicator lives in the bottom edge, whole, in its middle. The
# side columns are only as wide as the backplate's rounding, so the
# bottom edge is the width of the button less 8 px: stretched to it, the
# pill stays under the middle of the button, icon only or icon and label
# (a pill cut between the bottom corners, as before, came apart under a
# labelled button: one end at each side). Its width follows the button's:
# 6 or 16 px on a 48 px icon button, a short line under a labelled one.
#
#   python3 art/plasma.py           write the file
#   python3 art/plasma.py --check   exit 1 when the file in the tree is not current

import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "desktoptheme", "raye")
TASKS = os.path.join("widgets", "tasks.svg")

NAMES = ("topleft", "top", "topright", "left", "center", "right",
         "bottomleft", "bottom", "bottomright")
SIDES = ("top", "bottom", "left", "right")

# the colour classes Plasma fills in from the colour scheme (the values
# here are what an SVG viewer shows: Raye Night)
SCHEME = {
    "ColorScheme-Text": "#f1f0f4",
    "ColorScheme-Highlight": "#7b3cff",
}
# the soft glow under the pill of the app in use is the accent itself, and
# the colour of an app asking for attention is Windows' orange, drawn the
# Raye way
GLOW = "ColorScheme-Highlight"
AMBER = "#e0a458"

RADIUS = 4       # backplate corners, like in-page elements of Windows 11
SIDE = RADIUS    # width of the side columns, height of the top row
BOTTOM = 8       # height of the bottom row: the pill, its glow, 2 px under it
BUTTON = 48      # the icon button the drawing is made for
EDGE = BUTTON - 2 * SIDE  # width of the stretched middle column
MIDDLE_H = BUTTON - SIDE - BOTTOM  # height of the stretched middle row
MARGIN = 4       # content margins (the hint elements)
PILL_H = 3
PILL_BOTTOM = 2  # space under the pill
SHORT, LONG = 6, 16

# (colour, opacity) pairs: a class name or a fixed colour
TEXT = "ColorScheme-Text"
ACCENT = "ColorScheme-Highlight"
GREY = (TEXT, 0.55)

# prefix: backplate fill, pill width, pill colour, glow under the pill.
# Plasma asks for "<state>-hover" first when the pointer is on a button,
# then "hover", then "<state>"; a pinned app that is not running is
# "launcher-hover" on hover and has no frame otherwise.
STATES = (
    ("normal", None, SHORT, GREY, False),             # open, in the background
    ("focus", (TEXT, 0.06), LONG, (ACCENT, 1), True),  # the one in use
    ("attention", (AMBER, 0.22), LONG, (AMBER, 1), False),
    ("minimized", None, SHORT, GREY, False),
    ("hover", (TEXT, 0.06), 0, None, False),
    ("launcher-hover", (TEXT, 0.06), 0, None, False),
    ("normal-hover", (TEXT, 0.06), SHORT, GREY, False),
    ("focus-hover", (TEXT, 0.09), LONG, (ACCENT, 1), True),
    ("attention-hover", (AMBER, 0.30), LONG, (AMBER, 1), False),
    ("minimized-hover", (TEXT, 0.06), SHORT, GREY, False),
)

# the glow: two soft layers wider and taller than the pill, still inside
# the bottom edge (7 px tall, centred on the pill, ends at the bottom)
GLOW_LAYERS = ((LONG + 6, 7, 0.14), (LONG + 3, 5, 0.18))


def num(v):
    v = round(float(v), 3)
    return ("%d" % v) if v == int(v) else ("%g" % v)


def paint(colour, opacity):
    """fill attributes for a class colour or a fixed one"""
    if colour.startswith("ColorScheme-"):
        out = 'fill="currentColor" class="%s"' % colour
    else:
        out = 'fill="%s"' % colour
    if opacity != 1:
        out += ' fill-opacity="%s"' % num(opacity)
    return out


def box(x, y, w, h):
    return '<rect x="%s" y="%s" width="%s" height="%s" fill="#000" fill-opacity="0"/>' % (
        num(x), num(y), num(w), num(h))


def rect(x, y, w, h, fill):
    return '<rect x="%s" y="%s" width="%s" height="%s" %s/>' % (num(x), num(y), num(w), num(h), fill)


def corner_path(x, y, w, h, r, where):
    """A w x h box with its outer corner rounded by r."""
    if where == "topleft":
        return "M%s,%s H%s V%s H%s V%s A%s,%s 0 0 1 %s,%s Z" % tuple(
            map(num, (x + r, y, x + w, y + h, x, y + r, r, r, x + r, y)))
    if where == "topright":
        return "M%s,%s H%s A%s,%s 0 0 1 %s,%s V%s H%s Z" % tuple(
            map(num, (x, y, x + w - r, r, r, x + w, y + r, y + h, x)))
    if where == "bottomleft":
        return "M%s,%s H%s V%s H%s A%s,%s 0 0 1 %s,%s Z" % tuple(
            map(num, (x, y, x + w, y + h, x + r, r, r, x, y + h - r)))
    return "M%s,%s H%s V%s A%s,%s 0 0 1 %s,%s H%s Z" % tuple(
        map(num, (x, y, x + w, y + h - r, r, r, x + w - r, y + h, x)))


def capsule(cx, top, width, height, fill):
    """A pill centred on x = cx."""
    r = height / 2
    return '<rect x="%s" y="%s" width="%s" height="%s" rx="%s" ry="%s" %s/>' % (
        tuple(map(num, (cx - width / 2, top, width, height, r, r))) + (fill,))


def indicator(x, y, width, colour, glow):
    """The pill (and its glow) in the bottom edge piece at x, y. The
    piece is stretched to the button's width less the side columns, so
    the pill keeps the middle of the button and grows with it."""
    if not width:
        return []
    pill_top = BOTTOM - PILL_BOTTOM - PILL_H
    centre = pill_top + PILL_H / 2
    cx = x + EDGE / 2
    out = []
    if glow:
        for w, h, alpha in GLOW_LAYERS:
            out.append(capsule(cx, y + centre - h / 2, w, h, paint(GLOW, alpha)))
    out.append(capsule(cx, y + pill_top, width, PILL_H, paint(*colour)))
    return out


def frame(prefix, x0, backplate, width, colour, glow):
    """The nine pieces of one task button state, starting at x0."""
    s, e, m, b = SIDE, EDGE, MIDDLE_H, BOTTOM
    pos = {"topleft": (0, 0, s, s), "top": (s, 0, e, s), "topright": (s + e, 0, s, s),
           "left": (0, s, s, m), "center": (s, s, e, m), "right": (s + e, s, s, m),
           "bottomleft": (0, s + m, s, b), "bottom": (s, s + m, e, b),
           "bottomright": (s + e, s + m, s, b)}
    out = []
    for name in NAMES:
        x, y, w, h = pos[name]
        x += x0
        inner = [box(x, y, w, h)]
        if backplate:
            fill = paint(*backplate)
            if name in ("topleft", "topright", "bottomleft", "bottomright"):
                inner.append('<path d="%s" %s/>' % (corner_path(x, y, w, h, RADIUS, name), fill))
            else:
                inner.append(rect(x, y, w, h, fill))
        if name == "bottom":
            inner += indicator(x, y, width, colour, glow)
        out.append('<g id="%s-%s">' % (prefix, name) + "".join(inner) + "</g>")
    return out, 2 * s + e


def tasks():
    """Task buttons like the Windows 11 taskbar: the icon fills the
    button, a short pill under it (long, accent and glowing for the app
    in use, a short grey one for the others), a soft backplate with 4 px
    corners on hover and under the app in use.

    The content margins come from the hint elements, not from the size
    of the pieces, which is what made the icons tiny before."""
    body, x = [], 0
    for prefix, backplate, width, colour, glow in STATES:
        pieces, w = frame(prefix, x, backplate, width, colour, glow)
        body += pieces
        # content margins, and borders that stretch instead of repeating
        for side in SIDES:
            body.append('<g id="%s-hint-%s-margin">%s</g>' % (prefix, side, box(x, 0, MARGIN, MARGIN)))
        body.append('<g id="%s-hint-stretch-borders">%s</g>' % (prefix, box(x, 0, 1, 1)))
        x += w + 4
    body.append('<g id="hint-stretch-borders">%s</g>' % box(0, 0, 1, 1))
    style = "".join(".%s { color: %s; }" % (k, v) for k, v in SCHEME.items())
    height = BUTTON
    return ('<?xml version="1.0" encoding="UTF-8"?>\n'
            "<!-- Generated by art/plasma.py; edit that file, not this one. -->\n"
            '<svg xmlns="http://www.w3.org/2000/svg" width="%s" height="%s" viewBox="0 0 %s %s">\n'
            '<defs><style type="text/css" id="current-color-scheme">%s</style></defs>\n'
            % (num(x), num(height), num(x), num(height), style)
            + "\n".join(body) + "\n</svg>\n")


def render_all():
    """Every file this script makes, by path under desktoptheme/raye."""
    return {TASKS: tasks()}


def stale(out=OUT):
    """The files that differ from what this script makes."""
    bad = []
    for path, text in render_all().items():
        full = os.path.join(out, path)
        try:
            with open(full, encoding="utf-8") as f:
                if f.read() == text:
                    continue
        except OSError:
            pass
        bad.append(path)
    return bad


def write(out=OUT):
    for path, text in render_all().items():
        full = os.path.join(out, path)
        os.makedirs(os.path.dirname(full), exist_ok=True)
        with open(full, "w", encoding="utf-8") as f:
            f.write(text)


def main(argv):
    if "--check" in argv[1:]:
        bad = stale()
        for path in bad:
            print("not current: desktoptheme/raye/%s (run python3 art/plasma.py)" % path)
        return 1 if bad else 0
    write()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
