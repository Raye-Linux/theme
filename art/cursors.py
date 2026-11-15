#!/usr/bin/env python3
#
# Draws the Raye pointers from code and writes both pointer themes:
#
#   cursors/Raye/cursors/<name>             Xcursor files, sizes 24 to 96
#   cursors/Raye/cursors_scalable/<name>/   SVG pointers for Wayland, with
#                                           their metadata.json
#   cursors/Raye-Dark/...                   the same with a dark body
#
#   python3 art/cursors.py
#   python3 art/cursors.py --sheet out.png   contact sheet of the files
#
# Needs rsvg-convert (librsvg2-bin) and Pillow. The output is committed,
# so the package build needs neither; art/test_cursors.py checks it.
#
# The folders install next to the icon themes of the same name
# (/usr/share/icons/Raye, /usr/share/icons/Raye-Dark) and share their
# index.theme, the way one theme folder carries icons and pointers.
#
# Every drawing is on a 32 unit canvas; at the nominal size 24 one unit
# is one pixel, and the arrow is 19 px tall, like the Windows arrow. A
# shape is a list of parts drawn three times: the outline colour wide
# (the outline), the body colour (the shape), then the details on top.
# Other code (the pointer colour picker) builds its own colours with
# svg() and write_theme().

import io
import json
import math
import os
import shutil
import struct
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass, field

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "cursors")
CANVAS = 32
NOMINAL = 24
SIZES = (24, 32, 36, 48, 64, 72, 96)
OUTLINE = 1.1          # width of the outline outside the body, in units
FRAMES = 24           # one turn of the busy ring a second, 15 degrees a frame
FRAME_MS = 40


def _brand() -> dict:
    with open(os.path.join(ROOT, "tokens", "raye-tokens.json")) as f:
        return json.load(f)["brand"]


_B = _brand()
ACCENT, GLOW, INK = _B["violet"], _B["magenta"], _B["night.to"]


@dataclass(frozen=True)
class Colours:
    body: str
    outline: str
    accent: str = ACCENT
    glow: str = GLOW
    # the body fades to this colour at the bottom of the canvas; None: flat
    body_end: str | None = None


THEMES = {
    "Raye": Colours(body="#FFFFFF", outline=INK, body_end="#E9E3F5"),
    "Raye-Dark": Colours(body="#3A2C55", outline="#FFFFFF", body_end=INK),
}


# ------------------------------------------------------------------ parts

@dataclass
class Shape:
    """fills: closed paths; lines: (path, width) stroked with round ends;
    details: (path, width, role) drawn on top, role "outline", "body" or
    "accent"; dots: same, filled; extra: raw SVG drawn last;
    transform applies to everything."""
    hotspot: tuple[float, float]
    fills: list[str] = field(default_factory=list)
    lines: list[tuple[str, float]] = field(default_factory=list)
    details: list[tuple[str, float, str]] = field(default_factory=list)
    dots: list[tuple[str, str]] = field(default_factory=list)
    extra: list = field(default_factory=list)
    transform: str = ""


def _fmt(v: float) -> str:
    return f"{v:.2f}".rstrip("0").rstrip(".")


def poly(*pts: tuple[float, float]) -> str:
    return "M" + " L".join(f"{_fmt(x)} {_fmt(y)}" for x, y in pts) + " Z"


def circle(cx: float, cy: float, r: float) -> str:
    return (f"M{_fmt(cx - r)} {_fmt(cy)} A{_fmt(r)} {_fmt(r)} 0 1 0 {_fmt(cx + r)} {_fmt(cy)}"
            f" A{_fmt(r)} {_fmt(r)} 0 1 0 {_fmt(cx - r)} {_fmt(cy)} Z")


def rrect(x: float, y: float, w: float, h: float, r: float) -> str:
    return (f"M{_fmt(x + r)} {_fmt(y)} H{_fmt(x + w - r)} A{_fmt(r)} {_fmt(r)} 0 0 1 {_fmt(x + w)} {_fmt(y + r)}"
            f" V{_fmt(y + h - r)} A{_fmt(r)} {_fmt(r)} 0 0 1 {_fmt(x + w - r)} {_fmt(y + h)}"
            f" H{_fmt(x + r)} A{_fmt(r)} {_fmt(r)} 0 0 1 {_fmt(x)} {_fmt(y + h - r)}"
            f" V{_fmt(y + r)} A{_fmt(r)} {_fmt(r)} 0 0 1 {_fmt(x + r)} {_fmt(y)} Z")


def rotate(pt: tuple[float, float], deg: float) -> tuple[float, float]:
    a = math.radians(deg)
    x, y = pt[0] - CANVAS / 2, pt[1] - CANVAS / 2
    return (CANVAS / 2 + x * math.cos(a) - y * math.sin(a),
            CANVAS / 2 + x * math.sin(a) + y * math.cos(a))


# the arrow: tip at (4, 4), 19.4 units tall
ARROW = poly((4, 4), (4, 21), (8.2, 17.1), (11.1, 23.4), (13.9, 22.2),
             (11.1, 16.0), (16.5, 16.0))
ARROW_TIP = (3.6, 3.0)


def arrow(**kw) -> Shape:
    return Shape(hotspot=ARROW_TIP, fills=[ARROW], **kw)


# badges sit at the lower right of the arrow, on an accent disc
BX, BY, BR = 22.6, 23.0, 5.3


def badge(glyph: list[tuple[str, float]], dots: list[str] = ()) -> Shape:
    s = arrow()
    s.extra.append(("badge", glyph, list(dots)))
    return s


def ring_frame(cx: float, cy: float, r: float, t: float, frame: int,
               c: Colours) -> str:
    """One frame of the busy ring: the body-coloured track, and an arc
    going from accent to glow that turns 30 degrees a frame and breathes
    between a short and a long sweep."""
    o = c.outline
    out = [f'<circle cx="{_fmt(cx)}" cy="{_fmt(cy)}" r="{_fmt(r)}" fill="none" '
           f'stroke="{o}" stroke-width="{_fmt(t + 2 * OUTLINE)}"/>',
           f'<circle cx="{_fmt(cx)}" cy="{_fmt(cy)}" r="{_fmt(r)}" fill="none" '
           f'stroke="{_paint(c)}" stroke-width="{_fmt(t)}"/>']
    sweep = 100 + 60 * math.sin(2 * math.pi * frame / FRAMES)
    start = frame * 360 / FRAMES - 90
    steps = 10
    for i in range(steps):
        a0 = math.radians(start + sweep * i / steps)
        a1 = math.radians(start + sweep * (i + 1) / steps)
        col = _mix(c.accent, c.glow, i / (steps - 1))
        x0, y0 = cx + r * math.cos(a0), cy + r * math.sin(a0)
        x1, y1 = cx + r * math.cos(a1), cy + r * math.sin(a1)
        out.append(f'<path d="M{_fmt(x0)} {_fmt(y0)} A{_fmt(r)} {_fmt(r)} 0 0 1 '
                   f'{_fmt(x1)} {_fmt(y1)}" fill="none" stroke="{col}" '
                   f'stroke-width="{_fmt(t * 0.78)}" stroke-linecap="round"/>')
    return "\n".join(out)


def _mix(a: str, b: str, f: float) -> str:
    ca = [int(a[i:i + 2], 16) for i in (1, 3, 5)]
    cb = [int(b[i:i + 2], 16) for i in (1, 3, 5)]
    return "#" + "".join(f"{round(x + (y - x) * f):02X}" for x, y in zip(ca, cb))


def double_arrow(deg: float, length: float = 11) -> Shape:
    """Resize arrow along the vertical, turned by deg around the centre."""
    c, h, sw, hw = 16, length, 1.3, 4.4
    pts = [(c, c - h), (c + hw, c - h + 5), (c + sw, c - h + 5), (c + sw, c + h - 5),
           (c + hw, c + h - 5), (c, c + h), (c - hw, c + h - 5), (c - sw, c + h - 5),
           (c - sw, c - h + 5), (c - hw, c - h + 5)]
    return Shape(hotspot=(16, 16), fills=[poly(*[rotate(p, deg) for p in pts])])


def split(deg: float) -> Shape:
    """Column / row resize: two bars and arrows going out both ways."""
    pts = [(5.5, 16), (10, 11.8), (10, 14.7), (22, 14.7), (22, 11.8), (26.5, 16),
           (22, 20.2), (22, 17.3), (10, 17.3), (10, 20.2)]
    bars = [rrect(13.4, 7, 2.2, 18, 0.6), rrect(16.4, 7, 2.2, 18, 0.6)]
    s = Shape(hotspot=(16, 16), fills=[poly(*pts)] + bars)
    return _rotated(s, deg) if deg else s


def _rotated(s: Shape, deg: float) -> Shape:
    s.transform = f"rotate({_fmt(deg)} 16 16)"
    s.hotspot = rotate(s.hotspot, deg)
    return s


def pointer_arrow(deg: float) -> Shape:
    """Big single arrow pointing up, turned by deg (alternate select)."""
    s = Shape(hotspot=(16, 4.3),
              fills=[poly((16, 4.5), (23, 12), (18.3, 12), (18.3, 27), (13.7, 27),
                          (13.7, 12), (9, 12))])
    return _rotated(s, deg) if deg else s


def diagonal(t: tuple[float, float]):
    """Points along a 45 degree axis going up-right from t."""
    u = (1 / math.sqrt(2), -1 / math.sqrt(2))
    n = (1 / math.sqrt(2), 1 / math.sqrt(2))

    def at(d: float, side: float = 0.0) -> tuple[float, float]:
        return (t[0] + u[0] * d + n[0] * side, t[1] + u[1] * d + n[1] * side)
    return at


def pencil() -> Shape:
    at = diagonal((5, 27))
    h = 2.4
    body = poly(at(0), at(4.6, h), at(19, h), at(19, -h), at(4.6, -h))
    lead = poly(at(0), at(1.7, 0.9), at(1.7, -0.9))
    p0, p1 = at(4.6, h), at(4.6, -h)
    e0, e1 = at(16, h), at(16, -h)
    return Shape(hotspot=(5.4, 26.6), fills=[body],
                 details=[(f"M{_fmt(p0[0])} {_fmt(p0[1])} L{_fmt(p1[0])} {_fmt(p1[1])}", 0.8, "outline"),
                          (f"M{_fmt(e0[0])} {_fmt(e0[1])} L{_fmt(e1[0])} {_fmt(e1[1])}", 0.8, "outline")],
                 dots=[(lead, "outline")])


def dropper() -> Shape:
    at = diagonal((5, 27))
    d0, d1 = at(0.8), at(10)
    b0, b1 = at(10), at(16)
    bulb = at(18.5)
    return Shape(hotspot=(5.4, 26.6),
                 lines=[(f"M{_fmt(d0[0])} {_fmt(d0[1])} L{_fmt(d1[0])} {_fmt(d1[1])}", 2.2),
                        (f"M{_fmt(b0[0])} {_fmt(b0[1])} L{_fmt(b1[0])} {_fmt(b1[1])}", 4.6)],
                 fills=[circle(bulb[0], bulb[1], 3.0)],
                 dots=[(circle(*at(2.6), 0.75), "accent")])


def magnifier(plus: bool) -> Shape:
    glyph = [("M10.6 13.5 H16.4", 1.5)]
    if plus:
        glyph.append(("M13.5 10.6 V16.4", 1.5))
    return Shape(hotspot=(13.5, 13.5),
                 fills=[circle(13.5, 13.5, 7.2)],
                 lines=[("M19 19 L25.5 25.5", 3.4)],
                 details=[(d, w, "outline") for d, w in glyph]
                 + [(circle(13.5, 13.5, 7.2), 1.4, "outline")])


def hand_point() -> Shape:
    d = ("M12 5.2 A1.8 1.8 0 0 1 15.6 5.2 L15.6 12.6 A1.5 1.5 0 0 1 18.6 12.6"
         " L18.6 13.4 A1.5 1.5 0 0 1 21.6 13.4 L21.6 14.6 A1.4 1.4 0 0 1 24.4 14.6"
         " L24.4 21.5 C24.4 25 22.5 27.5 19 27.5 L16 27.5 C13.8 27.5 12.6 26.6 11.4 25"
         " L7.6 19.8 A1.6 1.6 0 0 1 10.2 17.8 L12 19.6 Z")
    return Shape(hotspot=(13.8, 3.6), fills=[d],
                 details=[("M15.6 12.6 V17.2", 0.8, "outline"),
                          ("M18.6 13.4 V17.6", 0.8, "outline"),
                          ("M21.6 14.6 V18", 0.8, "outline")])


def hand_open() -> Shape:
    return Shape(hotspot=(16, 15),
                 fills=[rrect(10, 14.5, 13.6, 12.5, 4.2)],
                 lines=[("M12.6 16 V8.6", 2.9), ("M15.8 15 V6.4", 2.9),
                        ("M19 15 V7.2", 2.9), ("M22 16.5 V10", 2.6),
                        ("M11.2 21.5 L7.2 16.8", 2.9)])


def hand_closed() -> Shape:
    """A fist seen from the back: four knuckles on top, the thumb folded
    across the front."""
    knuckles = [circle(x, 13.6, 1.85) for x in (12.1, 15.3, 18.5, 21.6)]
    return Shape(hotspot=(16, 16),
                 fills=[rrect(9.6, 13.4, 14, 13.2, 4.4)] + knuckles,
                 lines=[("M11.4 21.2 L8.6 18.2", 2.9)],
                 details=[("M13.7 12.6 V15.2", 0.8, "outline"), ("M16.9 12.6 V15.2", 0.8, "outline"),
                          ("M20.1 12.6 V15.2", 0.8, "outline"),
                          ("M11.2 20.4 C13 18.6 15.6 18.2 18.2 18.8", 0.85, "outline")])


def ibeam(deg: float = 0) -> Shape:
    d = "M13.3 7 Q16.5 7 16.5 9.4 Q16.5 7 19.7 7 M16.5 9.4 V22.6 M13.3 25 Q16.5 25 16.5 22.6 Q16.5 25 19.7 25"
    s = Shape(hotspot=(16.5, 16), lines=[(d, 1.5)])
    return _rotated(s, deg) if deg else s


def target() -> Shape:
    return Shape(hotspot=(16, 16),
                 lines=[(circle(16, 16, 7.4), 1.7),
                        ("M16 4 V9.6 M16 22.4 V28 M4 16 H9.6 M22.4 16 H28", 1.5)],
                 fills=[circle(16, 16, 1.5)],
                 dots=[(circle(16, 16, 1.5), "accent")])


def angle(deg: float) -> Shape:
    """Corner of a selection, top left turned by deg, with its point."""
    s = Shape(hotspot=(9, 9), lines=[("M9 22 V9 H22", 2.6)],
              dots=[(circle(9, 9, 1.2), "accent")])
    return _rotated(s, deg) if deg else s


def not_allowed() -> Shape:
    return Shape(hotspot=(16, 16),
                 lines=[(circle(16, 16, 8.4), 2.8),
                        ("M10.3 10.3 L21.7 21.7", 2.8)])


_DRAWINGS: dict = {}


def drawings() -> dict[str, Shape | str]:
    if not _DRAWINGS:
        _DRAWINGS.update(_build())
    return _DRAWINGS


def _build() -> dict[str, Shape | str]:
    """Every drawing by its file name. A string value means an animated
    pointer built by ring_frame()."""
    glyph_plus = [("M22.6 20.3 V25.7 M19.9 23 H25.3", 1.6)]
    glyph_help = [("M20.8 21.2 A1.85 1.85 0 1 1 23.6 22.7 C22.9 23.1 22.6 23.5 22.6 24.2", 1.45)]
    glyph_alias = [("M20.3 25.4 C20.3 22.4 21.6 21.3 24.6 21.3 M23.1 19.8 L24.7 21.3 L23.1 22.8", 1.35)]
    glyph_link = [(circle(21.4, 24.2, 1.55), 1.2), (circle(23.8, 21.8, 1.55), 1.2)]
    glyph_forbid = [(circle(22.6, 23, 3.0), 1.3), ("M20.5 20.9 L24.7 25.1", 1.3)]
    menu = arrow()
    menu.extra.append(("menu",))
    return {
        "default": arrow(),
        "right_ptr": _mirrored(arrow()),
        "pointer": hand_point(),
        "text": ibeam(),
        "vertical-text": ibeam(90),
        "wait": "wait",
        "progress": "progress",
        "help": badge(glyph_help, [circle(22.6, 26.0, 0.85)]),
        "alias": badge(glyph_alias),
        "copy": badge(glyph_plus),
        "link": badge(glyph_link),
        "dnd-no-drop": badge(glyph_forbid),
        "context-menu": menu,
        "crosshair": Shape(hotspot=(16, 16),
                           lines=[("M16 5 V13.6 M16 18.4 V27 M5 16 H13.6 M18.4 16 H27", 1.4),
                                  ("M16 16 V16.01", 1.6)]),
        "cell": Shape(hotspot=(16, 16), lines=[("M16 7 V25 M7 16 H25", 3.4)]),
        "not-allowed": not_allowed(),
        "all-scroll": Shape(hotspot=(16, 16),
                            lines=[("M16 8 V24 M8 16 H24", 2.4)],
                            fills=[poly((16, 4), (20, 8.8), (12, 8.8)),
                                   poly((16, 28), (20, 23.2), (12, 23.2)),
                                   poly((4, 16), (8.8, 12), (8.8, 20)),
                                   poly((28, 16), (23.2, 12), (23.2, 20))]),
        "openhand": hand_open(),
        "closedhand": hand_closed(),
        "size_ver": double_arrow(0),
        "size_hor": double_arrow(90),
        "size_bdiag": double_arrow(45, 10),
        "size_fdiag": double_arrow(-45, 10),
        "col-resize": split(0),
        "row-resize": split(90),
        "zoom-in": magnifier(True),
        "zoom-out": magnifier(False),
        "pencil": pencil(),
        "color-picker": dropper(),
        "up-arrow": pointer_arrow(0),
        "right-arrow": pointer_arrow(90),
        "down-arrow": pointer_arrow(180),
        "left-arrow": pointer_arrow(270),
        "target": target(),
        "ul_angle": angle(0),
        "ur_angle": angle(90),
        "lr_angle": angle(180),
        "ll_angle": angle(270),
        "x-cursor": Shape(hotspot=(16, 16),
                          lines=[("M9.5 9.5 L22.5 22.5 M22.5 9.5 L9.5 22.5", 3.2)]),
    }


def _mirrored(s: Shape) -> Shape:
    s.transform = f"matrix(-1 0 0 1 {CANVAS} 0)"
    s.hotspot = (CANVAS - s.hotspot[0], s.hotspot[1])
    return s


# names that show one of the drawings; hashes are the ones X toolkits ask for
ALIASES = {
    # a drag that moves shows the plain arrow, as on Windows
    "default": ["arrow", "left_ptr", "top_left_arrow", "wayland-cursor",
                "dnd-move", "dnd-none", "4498f0e0c1937ffe01fd06f973665830",
                "9081237383d90e509aa00f00170e968f"],
    "pointer": ["hand1", "hand2", "pointing_hand",
                "9d800788f1b08800ae810202380a0822", "e29285e634086352946a0e7090d73106"],
    "text": ["xterm", "ibeam"],
    "wait": ["watch"],
    "progress": ["left_ptr_watch", "half-busy",
                 "00000000000000020006000e7e9ffc3f", "08e8e1c95fe2fc01f976f1e063a24ccd",
                 "3ecb610c1bf2410f44200f48c40d3599"],
    "help": ["question_arrow", "whats_this", "left_ptr_help",
             "5c6cd98b3f3ebcb1f9c7f1c204630408", "d9ce0ab605698f320427677b458ad60b"],
    "crosshair": ["cross", "tcross", "dotbox", "icon"],
    "not-allowed": ["forbidden", "crossed_circle", "circle",
                    "03b6e0fcb3499374a867c041f52298f0"],
    "dnd-no-drop": ["no-drop"],
    "context-menu": ["dnd-ask"],
    "all-scroll": ["fleur", "size_all", "move"],
    "openhand": ["grab"],
    "closedhand": ["grabbing"],
    "size_ver": ["ns-resize", "n-resize", "s-resize", "v_double_arrow",
                 "sb_v_double_arrow", "top_side", "bottom_side", "top_tee", "bottom_tee",
                 "based_arrow_up", "based_arrow_down",
                 "00008160000006810000408080010102"],
    "size_hor": ["ew-resize", "e-resize", "w-resize", "h_double_arrow",
                 "sb_h_double_arrow", "left_side", "right_side", "left_tee", "right_tee",
                 "028006030e0e7ebffc7f7070c0600140"],
    "size_bdiag": ["nesw-resize", "ne-resize", "sw-resize", "top_right_corner",
                   "bottom_left_corner", "fcf1c3c7cd4491d801f1e1c78f100000"],
    "size_fdiag": ["nwse-resize", "nw-resize", "se-resize", "top_left_corner",
                   "bottom_right_corner", "c7088f0f3e6c8088236ef8e1e3e70000"],
    "col-resize": ["split_h", "14fef782d02440884392942c11205230"],
    "row-resize": ["split_v", "2870a09082c103050810ffdffffe0204"],
    "alias": ["dnd-link", "3085a0e285430894940527032f8b26df", "640fb0e74195791501fd1ed57b41487f",
              "a2a266d0498c3104214a47bd64ab0fc8"],
    "copy": ["dnd-copy", "1081e37283d90000800003c07f3ef6bf",
             "6407b0e94181790501fd1e167b474872", "b66166c04f8c3109214a4fbd64a50fc8"],
    "cell": ["plus"],
    "pencil": ["draft"],
    "x-cursor": ["X_cursor", "pirate", "kill"],
    "up-arrow": ["center_ptr", "sb_up_arrow"],
    "down-arrow": ["sb_down_arrow"],
    "left-arrow": ["sb_left_arrow"],
    "right-arrow": ["sb_right_arrow"],
}


# ------------------------------------------------------------------ SVG

def _paint(c: Colours) -> str:
    return "url(#b)" if c.body_end else c.body


def _body_gradient(c: Colours) -> str:
    """Top to bottom of the canvas, so every pointer gets the same light."""
    if not c.body_end:
        return ""
    return (f'<linearGradient id="b" gradientUnits="userSpaceOnUse" x1="0" y1="4" x2="0" y2="28">'
            f'<stop offset="0" stop-color="{c.body}"/><stop offset="1" stop-color="{c.body_end}"/>'
            f'</linearGradient>')


def _shadow_filter() -> str:
    return ('<filter id="s" x="-20%" y="-20%" width="140%" height="140%" '
            'color-interpolation-filters="sRGB">'
            '<feGaussianBlur stdDeviation="0.6"/></filter>')


def _layers(s: Shape, c: Colours) -> tuple[list[str], list[str]]:
    """(outline layer, everything) of a shape: the outline layer alone is
    its silhouette, used for the shadow."""
    o, b, w = c.outline, _paint(c), OUTLINE
    out, body, top = [], [], []
    join = 'stroke-linejoin="round" stroke-linecap="round"'
    for d in s.fills:
        out.append(f'<path d="{d}" fill="{o}" stroke="{o}" stroke-width="{_fmt(2 * w)}" {join}/>')
        body.append(f'<path d="{d}" fill="{b}"/>')
    for d, lw in s.lines:
        out.append(f'<path d="{d}" fill="none" stroke="{o}" stroke-width="{_fmt(lw + 2 * w)}" {join}/>')
        body.append(f'<path d="{d}" fill="none" stroke="{b}" stroke-width="{_fmt(lw)}" {join}/>')
    role = {"outline": o, "body": b, "accent": c.accent}
    for d, lw, r in s.details:
        top.append(f'<path d="{d}" fill="none" stroke="{role[r]}" stroke-width="{_fmt(lw)}" {join}/>')
    for d, r in s.dots:
        top.append(f'<path d="{d}" fill="{role[r]}"/>')
    for e in s.extra:
        if e[0] == "badge":
            _, glyph, dots = e
            out.append(f'<circle cx="{BX}" cy="{BY}" r="{_fmt(BR + w)}" fill="{o}"/>')
            top.append(f'<circle cx="{BX}" cy="{BY}" r="{BR}" fill="{c.accent}"/>')
            for d, lw in glyph:
                top.append(f'<path d="{d}" fill="none" stroke="#FFFFFF" stroke-width="{_fmt(lw)}" {join}/>')
            for d in dots:
                top.append(f'<path d="{d}" fill="#FFFFFF"/>')
        elif e[0] == "menu":
            box = rrect(17.6, 18.2, 10, 9.4, 1.6)
            out.append(f'<path d="{box}" fill="{o}" stroke="{o}" stroke-width="{_fmt(2 * w)}" {join}/>')
            top.append(f'<path d="{box}" fill="{c.accent}"/>')
            top.append(f'<path d="M19.8 20.9 H25.4 M19.8 22.9 H25.4 M19.8 24.9 H23.4" '
                       f'fill="none" stroke="#FFFFFF" stroke-width="1.1" {join}/>')
    return out, out + body + top


def svg(name: str, c: Colours, frame: int = 0) -> str:
    """The SVG of one pointer (one frame of an animated one)."""
    s = drawings()[name]
    if s == "wait":
        art = [ring_frame(16, 16, 7.4, 3.0, frame, c)]
        shadow = [f'<circle cx="16" cy="16" r="7.4" fill="none" stroke="#000" '
                  f'stroke-width="{_fmt(3.0 + 2 * OUTLINE)}"/>']
    elif s == "progress":
        shadow, art = _layers(arrow(), c)
        cx, cy, r, t = RING_AT
        art = art + [ring_frame(cx, cy, r, t, frame, c)]
        shadow = shadow + [f'<circle cx="{_fmt(cx)}" cy="{_fmt(cy)}" r="{_fmt(r)}" fill="none" '
                           f'stroke="#000" stroke-width="{_fmt(t + 2 * OUTLINE)}"/>']
    else:
        shadow, art = _layers(s, c)
    tr = s.transform if isinstance(s, Shape) else ""
    g = f'<g transform="{tr}">' if tr else "<g>"
    return "\n".join([
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{CANVAS}" height="{CANVAS}" '
        f'viewBox="0 0 {CANVAS} {CANVAS}">',
        f"<defs>{_shadow_filter()}{_body_gradient(c)}</defs>",
        f'<g filter="url(#s)" opacity="0.28" transform="translate(0 0.7)">{g}'
        + "".join(shadow).replace(c.outline, "#000000") + "</g></g>",
        g, *art, "</g>", "</svg>", ""])


# The busy ring of "progress" on its own, at the place it has next to the
# arrow. The desktop draws it beside the pointer while an app starts, so
# whatever the pointer is (arrow, text beam, hand) it gets the same ring
# as the progress pointer. Frames and metadata like a scalable cursor;
# no Xcursor file, nothing asks for it by name.
RING = "raye-launch-ring"
RING_AT = (22.4, 22.6, 4.6, 2.3)


def ring_svg(c: Colours, frame: int = 0) -> str:
    cx, cy, r, t = RING_AT
    return "\n".join([
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{CANVAS}" height="{CANVAS}" '
        f'viewBox="0 0 {CANVAS} {CANVAS}">',
        f"<defs>{_shadow_filter()}{_body_gradient(c)}</defs>",
        f'<g filter="url(#s)" opacity="0.28" transform="translate(0 0.7)">'
        f'<circle cx="{_fmt(cx)}" cy="{_fmt(cy)}" r="{_fmt(r)}" fill="none" '
        f'stroke="#000000" stroke-width="{_fmt(t + 2 * OUTLINE)}"/></g>',
        ring_frame(cx, cy, r, t, frame, c), "</svg>", ""])


def write_ring(sdir: str, c: Colours) -> None:
    d = os.path.join(sdir, RING)
    if os.path.isdir(d):
        shutil.rmtree(d)
    os.makedirs(d)
    meta = []
    for f in range(FRAMES):
        fn = f"{RING}-{f + 1:02d}.svg"
        with open(os.path.join(d, fn), "w") as fh:
            fh.write(ring_svg(c, f))
        meta.append({"filename": fn, "hotspot_x": ARROW_TIP[0], "hotspot_y": ARROW_TIP[1],
                     "nominal_size": NOMINAL, "delay": FRAME_MS})
    with open(os.path.join(d, "metadata.json"), "w") as fh:
        json.dump(meta, fh, indent=4)
        fh.write("\n")


def hotspot(name: str) -> tuple[float, float]:
    s = drawings()[name]
    if s == "wait":
        return (16.0, 16.0)
    if s == "progress":
        return ARROW_TIP
    return s.hotspot


def frames(name: str) -> int:
    return FRAMES if drawings()[name] in ("wait", "progress") else 1


# ------------------------------------------------------------------ Xcursor

XCURSOR_IMAGE = 0xFFFD0002


def rasterize(svg_text: str, px: int) -> bytes:
    """RGBA bytes (straight alpha) of the SVG at px x px."""
    from PIL import Image
    png = subprocess.run(["rsvg-convert", "-w", str(px), "-h", str(px)],
                         input=svg_text.encode(), capture_output=True, check=True).stdout
    img = Image.open(io.BytesIO(png)).convert("RGBA")
    if img.size != (px, px):
        raise ValueError(f"rsvg-convert gave {img.size}, wanted {px}")
    return img.tobytes()


def image_px(size: int) -> int:
    return round(CANVAS * size / NOMINAL)


def hotspot_px(h: tuple[float, float], size: int) -> tuple[int, int]:
    s = size / NOMINAL
    px = image_px(size)
    return (min(px - 1, int(h[0] * s)), min(px - 1, int(h[1] * s)))


def _premultiplied_argb(rgba: bytes) -> bytes:
    out = bytearray(len(rgba))
    for i in range(0, len(rgba), 4):
        r, g, b, a = rgba[i], rgba[i + 1], rgba[i + 2], rgba[i + 3]
        # little-endian ARGB32 is B, G, R, A in memory
        out[i] = (b * a + 127) // 255
        out[i + 1] = (g * a + 127) // 255
        out[i + 2] = (r * a + 127) // 255
        out[i + 3] = a
    return bytes(out)


def xcursor(images: list[tuple[int, int, int, int, int, bytes]]) -> bytes:
    """Xcursor file from (nominal size, width, height, xhot, yhot, delay,
    RGBA bytes) tuples, in the order given."""
    header = 16
    toc = 12 * len(images)
    pos = header + toc
    chunks, entries = [], []
    for size, w, h, xh, yh, delay, rgba in images:
        chunk = struct.pack("<9I", 36, XCURSOR_IMAGE, size, 1, w, h, xh, yh, delay)
        chunk += _premultiplied_argb(rgba)
        entries.append(struct.pack("<3I", XCURSOR_IMAGE, size, pos))
        chunks.append(chunk)
        pos += len(chunk)
    return (b"Xcur" + struct.pack("<3I", header, 0x10000, len(images))
            + b"".join(entries) + b"".join(chunks))


def read_xcursor(data: bytes) -> list[dict]:
    """Parses an Xcursor file; raises ValueError on anything malformed."""
    if data[:4] != b"Xcur":
        raise ValueError("no Xcur magic")
    header, version, ntoc = struct.unpack_from("<3I", data, 4)
    if header != 16 or ntoc == 0:
        raise ValueError("bad header")
    images = []
    for i in range(ntoc):
        typ, sub, pos = struct.unpack_from("<3I", data, header + 12 * i)
        if typ != XCURSOR_IMAGE:
            raise ValueError("unexpected chunk type")
        hs, t2, s2, ver, w, h, xh, yh, delay = struct.unpack_from("<9I", data, pos)
        if (hs, t2, s2, ver) != (36, typ, sub, 1):
            raise ValueError("chunk header does not match its toc entry")
        end = pos + 36 + 4 * w * h
        if end > len(data):
            raise ValueError("chunk runs past the end of the file")
        images.append({"size": sub, "width": w, "height": h, "xhot": xh,
                       "yhot": yh, "delay": delay, "pixels": data[pos + 36:end]})
    return images


# ------------------------------------------------------------------ output

def write_theme(dest: str, c: Colours, jobs: int = 8) -> None:
    """Writes cursors/ and cursors_scalable/ of one colour set into dest."""
    xdir = os.path.join(dest, "cursors")
    sdir = os.path.join(dest, "cursors_scalable")
    for d in (xdir, sdir):
        if os.path.isdir(d):
            shutil.rmtree(d)
        os.makedirs(d)
    names = list(drawings())

    def one(name: str) -> None:
        n = frames(name)
        delay = FRAME_MS if n > 1 else 0
        hot = hotspot(name)
        texts = [svg(name, c, f) for f in range(n)]
        images = []
        for size in SIZES:
            px = image_px(size)
            xh, yh = hotspot_px(hot, size)
            for t in texts:
                images.append((size, px, px, xh, yh, delay, rasterize(t, px)))
        with open(os.path.join(xdir, name), "wb") as f:
            f.write(xcursor(images))
        os.makedirs(os.path.join(sdir, name))
        meta = []
        for f, t in enumerate(texts):
            fn = f"{name}-{f + 1:02d}.svg" if n > 1 else f"{name}.svg"
            with open(os.path.join(sdir, name, fn), "w") as fh:
                fh.write(t)
            entry = {"filename": fn, "hotspot_x": round(hot[0], 2),
                     "hotspot_y": round(hot[1], 2), "nominal_size": NOMINAL}
            if n > 1:
                entry["delay"] = delay
            meta.append(entry)
        with open(os.path.join(sdir, name, "metadata.json"), "w") as fh:
            json.dump(meta, fh, indent=4)
            fh.write("\n")

    with ThreadPoolExecutor(jobs) as pool:
        list(pool.map(one, names))
    write_ring(sdir, c)
    for target, aliases in ALIASES.items():
        for a in aliases:
            os.symlink(target, os.path.join(xdir, a))
            os.symlink(target, os.path.join(sdir, a))


def sheet(path: str, sizes: tuple[int, ...] = (24, 36, 48)) -> None:
    """Contact sheet of the written Xcursor files, at 100, 150 and 200 %:
    each theme on Raye Night and Raye Day, the hotspot marked by a small
    red cross under the pointer, first frames only."""
    from PIL import Image, ImageDraw
    names = list(drawings())
    cols = 12
    rows = (len(names) + cols - 1) // cols
    bands = [(t, bg) for t in THEMES for bg in ("#141318", "#F5F4F7")]
    cells = [image_px(size) + 16 for size in sizes]
    img = Image.new("RGB", (cols * max(cells), sum(rows * c * len(bands) for c in cells)))
    draw = ImageDraw.Draw(img)
    y0 = 0
    for size, cell in zip(sizes, cells):
        for theme, bg in bands:
            draw.rectangle((0, y0, img.width, y0 + rows * cell), fill=bg)
            for i, name in enumerate(names):
                with open(os.path.join(OUT, theme, "cursors", name), "rb") as f:
                    im = next(x for x in read_xcursor(f.read()) if x["size"] == size)
                raw = bytearray(im["pixels"])
                for k in range(0, len(raw), 4):   # premultiplied BGRA -> RGBA
                    bb, g, r, a = raw[k:k + 4]
                    if a:
                        raw[k:k + 4] = bytes((min(255, r * 255 // a), min(255, g * 255 // a),
                                              min(255, bb * 255 // a), a))
                pic = Image.frombytes("RGBA", (im["width"], im["height"]), bytes(raw))
                x, y = (i % cols) * cell + 8, y0 + (i // cols) * cell + 8
                hx, hy = x + im["xhot"], y + im["yhot"]
                draw.line((hx - 4, hy, hx + 4, hy), fill="#FF3B5C")
                draw.line((hx, hy - 4, hx, hy + 4), fill="#FF3B5C")
                img.paste(pic, (x, y), pic)
            y0 += rows * cell
    img.save(path)


def main() -> int:
    if len(sys.argv) == 3 and sys.argv[1] == "--sheet":
        sheet(sys.argv[2])
        return 0
    if not shutil.which("rsvg-convert"):
        print("rsvg-convert not found (librsvg2-bin)", file=sys.stderr)
        return 1
    for theme, colours in THEMES.items():
        write_theme(os.path.join(OUT, theme), colours)
        print(theme, len(drawings()), "drawings,",
              sum(len(v) for v in ALIASES.values()), "aliases")
    return 0


if __name__ == "__main__":
    sys.exit(main())
