#!/usr/bin/env python3
#
# The Raye icon themes, drawn from code:
#
#   icons/Raye/index.theme            light theme (Raye Day)
#   icons/Raye-Dark/index.theme       dark theme (Raye Night)
#   icons/Raye/<context>/<size>/*.svg the drawings
#   icons/Raye-Dark/<context>/...     links to the Raye files, unless a
#                                     drawing has a dark version
#
#   python3 art/icons/lib.py
#
# Needs nothing but Python. The output is committed. Every module of
# art/icons that defines ICONS (places.py, mimetypes.py, and later the
# apps and symbolic sets) is drawn into it; art/icons/test_icons.py and
# art/icons/sheet.py check and show the result.
#
# A drawing is a function of the pixel size: it computes its own
# geometry for that size and snaps it to the pixel grid, and it leaves
# out what does not read at small sizes. A 16 px icon is drawn at 16,
# never scaled down from a big one.

import importlib
import json
import math
import os
import shutil
import sys
from dataclasses import dataclass, field
from typing import Callable

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
OUT = os.path.join(ROOT, "icons")

# fixed sizes, and the scalable drawing used from 64 px up
SIZES = (16, 22, 24, 32, 48)
SCALABLE = 64
MODULES = ("places", "mimetypes", "apps", "symbolic", "actions")

THEMES = {
    "Raye": {
        "name": "Raye", "name_fr": "Raye",
        "comment": "Raye Linux icons, light", "comment_fr": "Icônes de Raye Linux, claires",
        "inherits": "breeze,Papirus,hicolor",
    },
    "Raye-Dark": {
        "name": "Raye Dark", "name_fr": "Raye sombre",
        "comment": "Raye Linux icons, dark", "comment_fr": "Icônes de Raye Linux, sombres",
        "inherits": "breeze-dark,Papirus-Dark,hicolor",
    },
}
CONTEXTS = {"places": "Places", "devices": "Devices", "mimetypes": "MimeTypes",
            "apps": "Applications", "categories": "Categories",
            "status": "Status", "actions": "Actions"}


def _brand() -> dict:
    with open(os.path.join(ROOT, "tokens", "raye-tokens.json")) as f:
        return json.load(f)["brand"]


_B = _brand()

# colours of the drawings (theming 3.8)
VIOLET = _B["violet"]          # folder body
GLOW = _B["magenta"]
INK = _B["night.to"]
FOLDER_BODY = (VIOLET, "#9A6BFF")
FOLDER_BACK = ("#5B22D6", "#6A2EEB")
FLAP = ("#A27BFF", "#9A70FF", "#9264FF")
PAPER = ("#FFFFFF", "#F2F2F4")
PAPER_EDGE = "#B8B7BD"
GRAPHITE = ("#5E5C66", "#3F3E45", "#302F35")
METAL = ("#F3F2F5", "#CBCAD0")


# ------------------------------------------------------------------ SVG

def f(v: float) -> str:
    """Number for SVG text: short, no trailing zeros."""
    s = f"{v:.3f}".rstrip("0").rstrip(".")
    return "0" if s == "-0" else s


def snap(v: float) -> float:
    return float(round(v))


def half(v: float) -> float:
    """Centre of a pixel: where a 1 px line must sit to stay sharp."""
    return math.floor(v) + 0.5


def rrect(x: float, y: float, w: float, h: float, r: float) -> str:
    r = max(0.0, min(r, w / 2, h / 2))
    if r == 0:
        return f"M{f(x)} {f(y)}H{f(x + w)}V{f(y + h)}H{f(x)}Z"
    return (f"M{f(x + r)} {f(y)}H{f(x + w - r)}A{f(r)} {f(r)} 0 0 1 {f(x + w)} {f(y + r)}"
            f"V{f(y + h - r)}A{f(r)} {f(r)} 0 0 1 {f(x + w - r)} {f(y + h)}"
            f"H{f(x + r)}A{f(r)} {f(r)} 0 0 1 {f(x)} {f(y + h - r)}"
            f"V{f(y + r)}A{f(r)} {f(r)} 0 0 1 {f(x + r)} {f(y)}Z")


def circle(cx: float, cy: float, r: float) -> str:
    return (f"M{f(cx - r)} {f(cy)}A{f(r)} {f(r)} 0 1 0 {f(cx + r)} {f(cy)}"
            f"A{f(r)} {f(r)} 0 1 0 {f(cx - r)} {f(cy)}Z")


def poly(*pts: tuple[float, float]) -> str:
    return "M" + "L".join(f"{f(x)} {f(y)}" for x, y in pts) + "Z"


class Svg:
    """One icon at one pixel size."""

    def __init__(self, n: int):
        self.n = n
        self.defs: list[str] = []
        self.body: list[str] = []
        self._ids = 0

    def _id(self, prefix: str) -> str:
        self._ids += 1
        return f"{prefix}{self._ids}"

    def linear(self, x1: float, y1: float, x2: float, y2: float,
               stops: tuple[str, ...] | list[tuple[float, str, float]]) -> str:
        gid = self._id("g")
        self.defs.append(f'<linearGradient id="{gid}" gradientUnits="userSpaceOnUse" '
                         f'x1="{f(x1)}" y1="{f(y1)}" x2="{f(x2)}" y2="{f(y2)}">'
                         + self._stops(stops) + "</linearGradient>")
        return f"url(#{gid})"

    def radial(self, cx: float, cy: float, r: float,
               stops: tuple[str, ...] | list[tuple[float, str, float]]) -> str:
        gid = self._id("r")
        self.defs.append(f'<radialGradient id="{gid}" gradientUnits="userSpaceOnUse" '
                         f'cx="{f(cx)}" cy="{f(cy)}" r="{f(r)}">'
                         + self._stops(stops) + "</radialGradient>")
        return f"url(#{gid})"

    @staticmethod
    def _stops(stops) -> str:
        out = []
        if stops and isinstance(stops[0], str):
            k = len(stops) - 1
            stops = [(i / k if k else 0, c, 1.0) for i, c in enumerate(stops)]
        for off, col, op in stops:
            o = f' stop-opacity="{f(op)}"' if op != 1 else ""
            out.append(f'<stop offset="{f(off)}" stop-color="{col}"{o}/>')
        return "".join(out)

    def path(self, d: str, fill: str = "none", stroke: str | None = None,
             width: float | None = None, opacity: float = 1.0, cap: str = "round",
             extra: str = "") -> None:
        a = [f'd="{d}"', f'fill="{fill}"']
        if stroke:
            a += [f'stroke="{stroke}"', f'stroke-width="{f(width or 1)}"',
                  f'stroke-linecap="{cap}"', 'stroke-linejoin="round"']
        if opacity != 1:
            a.append(f'opacity="{f(opacity)}"')
        if extra:
            a.append(extra)
        self.body.append(f"<path {' '.join(a)}/>")

    def shadow(self, d: str, dy: float, opacity: float, blur: float = 0) -> None:
        """Soft shadow of a shape: a blurred copy below it at 32 px and
        more, a plain offset copy below that."""
        if blur and self.n >= 32:
            fid = self._id("s")
            self.defs.append(f'<filter id="{fid}" x="-10%" y="-10%" width="120%" height="130%" '
                             f'color-interpolation-filters="sRGB">'
                             f'<feGaussianBlur stdDeviation="{f(blur)}"/></filter>')
            self.body.append(f'<path d="{d}" fill="#000000" opacity="{f(opacity)}" '
                             f'transform="translate(0 {f(dy)})" filter="url(#{fid})"/>')
        else:
            self.body.append(f'<path d="{d}" fill="#000000" opacity="{f(opacity * 0.6)}" '
                             f'transform="translate(0 {f(dy)})"/>')

    def raw(self, text: str) -> None:
        self.body.append(text)

    def text(self) -> str:
        n = self.n
        defs = f"<defs>{''.join(self.defs)}</defs>" if self.defs else ""
        return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{n}" height="{n}" '
                f'viewBox="0 0 {n} {n}">{defs}{"".join(self.body)}</svg>\n')


# ------------------------------------------------------------------ glyphs

# Monoline glyphs on a 12 unit box, white on folders and badges. Each is
# a list of (kind, data): ("line", d), ("fill", d), ("dash", d).
GLYPHS: dict[str, list[tuple[str, str]]] = {
    "document": [("line", "M3.2 1.5H7.3L9.3 3.5V10.5H3.2Z"), ("line", "M5 6.2H7.5M5 8.4H7.5")],
    "download": [("line", "M6 1.5V7.8M3.2 5.2L6 8L8.8 5.2M2 10.5H10")],
    "music": [("fill", circle(4.2, 9.1, 1.75)), ("line", "M5.9 9.1V2L9.6 3.2V5")],
    "pictures": [("line", "M1.5 10L4.8 6.2L7.4 8.8L8.8 7.4L10.5 10Z"),
                 ("fill", circle(8.6, 3.4, 1.25))],
    "videos": [("fill", "M3.8 2.3L9.6 6L3.8 9.7Z")],
    "templates": [("dash", "M3 1.5H9V10.5H3Z"), ("line", "M5 5H7M5 7.4H7")],
    "publicshare": [("fill", circle(4.4, 3.8, 1.6)), ("fill", circle(8.4, 4.4, 1.3)),
                    ("line", "M1.6 10.3C1.6 7.4 7.2 7.4 7.2 10.3M8 7.4C9.6 7.4 10.6 8.3 10.6 10.3")],
    "games": [("line", "M3.8 3.4H8.2A2.6 2.6 0 0 1 8.2 8.6H3.8A2.6 2.6 0 0 1 3.8 3.4Z"),
              ("line", "M2.9 6H4.9M3.9 5V7"), ("fill", circle(7.9, 5.3, 0.6)),
              ("fill", circle(8.9, 6.7, 0.6))],
    "cloud": [("line", "M3.4 9.6H8.6A2.3 2.3 0 0 0 8.9 5A3.1 3.1 0 0 0 3.1 5.4A2.1 2.1 0 0 0 3.4 9.6Z")],
    "network": [("line", circle(6, 6, 4.5)),
                ("line", "M1.5 6H10.5M6 1.5C3.9 4 3.9 8 6 10.5C8.1 8 8.1 4 6 1.5")],
    "remote": [("line", "M2 4.2H9.6M7.6 2.2L9.6 4.2L7.6 6.2M10 7.8H2.4M4.4 5.8L2.4 7.8L4.4 9.8")],
    "recent": [("line", circle(6, 6, 4.5)), ("line", "M6 3.4V6L7.9 7.3")],
    "home": [("line", "M1.8 6.2L6 2.3L10.2 6.2M3 5.2V10.2H9V5.2"), ("line", "M5 10.2V7.6H7V10.2")],
    "desktop": [("line", "M2.2 2.2H9.8V8.2H2.2Z"), ("line", "M4.5 10.4H7.5M6 8.2V10.4")],
    "windows": [("line", "M2 2H10V10H2Z"), ("line", "M2 4.3H10"),
                ("line", "M3.4 7.3Q6 5.1 8.6 7.3Q6 9.5 3.4 7.3Z"), ("fill", circle(6, 7.3, 0.8))],
    "encrypted": [("line", "M6 1.5L9.8 3V6C9.8 8.4 8.2 9.9 6 10.7C3.8 9.9 2.2 8.4 2.2 6V3Z"),
                  ("fill", circle(6, 5.5, 1.05)), ("line", "M6 6.3V8")],
    "locked": [("line", "M2.6 5.6H9.4V10.4H2.6Z"), ("line", "M4 5.6V4A2 2 0 0 1 8 4V5.6"),
               ("fill", circle(6, 7.9, 0.8))],
    "star": [("line", "M6 1.8L7.3 4.6L10.3 4.9L8 6.9L8.7 9.9L6 8.3L3.3 9.9L4 6.9L1.7 4.9L4.7 4.6Z")],
}

# what a glyph keeps at 16 px, where a 6 px box has no room for detail
GLYPHS_SMALL: dict[str, list[tuple[str, str]]] = {
    "document": [("line", "M3 1.5H9V10.5H3Z"), ("line", "M5 6H7")],
    "templates": [("dash", "M3 1.5H9V10.5H3Z")],
    "publicshare": [("fill", circle(6, 4, 2)), ("line", "M2 10.5C2 7 10 7 10 10.5")],
    "games": [("line", "M3.6 3.2H8.4A2.8 2.8 0 0 1 8.4 8.8H3.6A2.8 2.8 0 0 1 3.6 3.2Z")],
    "network": [("line", circle(6, 6, 4.5)), ("line", "M1.5 6H10.5")],
    "windows": [("line", "M2 2H10V10H2Z"), ("fill", circle(6, 6.6, 1.6))],
    "encrypted": [("line", "M6 1.5L9.8 3V6C9.8 8.4 8.2 9.9 6 10.7C3.8 9.9 2.2 8.4 2.2 6V3Z")],
    "locked": [("fill", "M2.6 5.6H9.4V10.4H2.6Z"), ("line", "M4 5.6V4A2 2 0 0 1 8 4V5.6")],
    "remote": [("line", "M2 4H9.6M7.6 2L9.6 4L7.6 6M10 8H2.4M4.4 6L2.4 8L4.4 10")],
    "home": [("line", "M1.8 6.2L6 2.3L10.2 6.2M3 5.2V10.2H9V5.2")],
}


def stroke_px(n: int) -> float:
    """Glyph line width in pixels at an icon size."""
    return {16: 1.0, 22: 1.25, 24: 1.25, 32: 1.5, 48: 2.0}.get(n, n / 24)


def glyph(svg: Svg, name: str, cx: float, cy: float, box: float,
          colour: str = "#FFFFFF", opacity: float = 1.0) -> None:
    """Draws a glyph centred on (cx, cy) in a box of box pixels."""
    parts = GLYPHS_SMALL.get(name) if box <= 7 else None
    parts = parts or GLYPHS[name]
    k = box / 12
    w = stroke_px(svg.n) / k
    items = []
    for kind, d in parts:
        if kind == "fill":
            items.append(f'<path d="{d}" fill="{colour}"/>')
        else:
            dash = f' stroke-dasharray="{f(1.6)} {f(1.2)}"' if kind == "dash" else ""
            items.append(f'<path d="{d}" fill="none" stroke="{colour}" stroke-width="{f(w)}" '
                         f'stroke-linecap="round" stroke-linejoin="round"{dash}/>')
    op = f' opacity="{f(opacity)}"' if opacity != 1 else ""
    svg.raw(f'<g transform="translate({f(cx - box / 2)} {f(cy - box / 2)}) scale({f(k)})"{op}>'
            + "".join(items) + "</g>")


# ------------------------------------------------------------------ bases

@dataclass(frozen=True)
class FolderBox:
    left: float
    right: float
    tab_top: float
    back_top: float
    front_top: float
    bottom: float
    radius: float

    @property
    def flap_centre(self) -> tuple[float, float]:
        return ((self.left + self.right) / 2, (self.front_top + self.bottom) / 2 + 0.25)


def folder_box(n: int) -> FolderBox:
    m = max(1, round(n * 0.0625))
    return FolderBox(left=m, right=n - m, tab_top=round(n * 0.14),
                     back_top=round(n * 0.14) + max(1, round(n * 0.06)),
                     front_top=round(n * 0.31), bottom=n - max(2, round(n * 0.125)),
                     radius=max(1.0, n / 16))


def folder(n: int, mark: str | None = None, opened: bool = False,
           flap: tuple[str, ...] = FLAP) -> Svg:
    """The Raye folder: violet back with a tab, a sheet of paper, the
    lighter front flap, and a glyph on the flap."""
    s = Svg(n)
    b = folder_box(n)
    r = b.radius
    tab_w = round(n * 0.38)
    back = (f"M{f(b.left)} {f(b.tab_top + r)}A{f(r)} {f(r)} 0 0 1 {f(b.left + r)} {f(b.tab_top)}"
            f"H{f(b.left + tab_w - r)}L{f(b.left + tab_w + (b.back_top - b.tab_top))} {f(b.back_top)}"
            f"H{f(b.right - r)}A{f(r)} {f(r)} 0 0 1 {f(b.right)} {f(b.back_top + r)}"
            f"V{f(b.bottom - r)}A{f(r)} {f(r)} 0 0 1 {f(b.right - r)} {f(b.bottom)}"
            f"H{f(b.left + r)}A{f(r)} {f(r)} 0 0 1 {f(b.left)} {f(b.bottom - r)}Z")
    if n >= 32:
        s.shadow(back, n / 32, 0.28, blur=n / 40)
    elif n >= 22:
        s.shadow(back, 1, 0.3)
    s.path(back, fill=s.linear(0, b.tab_top, 0, b.bottom, FOLDER_BACK))
    if n >= 22:
        inset = max(2.0, round(n * 0.08))
        top = b.back_top + max(1, round(n * 0.04))
        paper = rrect(b.left + inset, top, b.right - b.left - 2 * inset,
                      b.bottom - top - 1, max(0.5, r / 2))
        s.path(paper, fill=s.linear(0, top, 0, b.front_top, PAPER))
    if opened:
        skew = round(n * 0.09)
        front = poly((b.left + skew, b.front_top + 1), (b.right, b.front_top + 1),
                     (b.right - skew, b.bottom), (b.left, b.bottom))
    else:
        front = rrect(b.left, b.front_top, b.right - b.left, b.bottom - b.front_top, r)
    s.path(front, fill=s.linear(b.left, b.front_top, b.right, b.bottom, flap))
    # 1 px light line along the top of the flap
    y = half(b.front_top + (1 if opened else 0))
    if not opened:
        s.path(f"M{f(b.left + r)} {f(y)}H{f(b.right - r)}", stroke="#FFFFFF", width=1,
               opacity=0.5, cap="butt")
    if mark:
        cx, cy = b.flap_centre
        box = round(n * 0.375) if n > 16 else 6
        glyph(s, mark, cx, cy, box, opacity=0.8 if n > 16 else 0.95)
    return s


@dataclass(frozen=True)
class PageBox:
    left: float
    right: float
    top: float
    bottom: float
    fold: float

    @property
    def centre(self) -> tuple[float, float]:
        return ((self.left + self.right) / 2, (self.top + self.bottom) / 2 + self.fold / 4)


def page_box(n: int) -> PageBox:
    side = {16: 3, 22: 4, 24: 4, 32: 6, 48: 9}.get(n, round(n * 0.19))
    top = {16: 1, 22: 2, 24: 2, 32: 2, 48: 3}.get(n, round(n * 0.0625))
    return PageBox(left=side, right=n - side, top=top, bottom=n - top,
                   fold=round(n * 0.25))


def page(n: int, lines: bool = False) -> tuple[Svg, PageBox]:
    """A sheet of paper with its top right corner folded."""
    s = Svg(n)
    p = page_box(n)
    r = max(0.5, n / 32)
    c = p.fold
    sheet = (f"M{f(p.left + r)} {f(p.top)}H{f(p.right - c)}L{f(p.right)} {f(p.top + c)}"
             f"V{f(p.bottom - r)}A{f(r)} {f(r)} 0 0 1 {f(p.right - r)} {f(p.bottom)}"
             f"H{f(p.left + r)}A{f(r)} {f(r)} 0 0 1 {f(p.left)} {f(p.bottom - r)}"
             f"V{f(p.top + r)}A{f(r)} {f(r)} 0 0 1 {f(p.left + r)} {f(p.top)}Z")
    if n >= 32:
        s.shadow(sheet, n / 40, 0.25, blur=n / 48)
    s.path(sheet, fill=s.linear(p.left, p.top, p.right, p.bottom, PAPER))
    # the edge: a 1 px line inside the sheet, so it shows on light backgrounds
    e = 0.5
    edge = (f"M{f(p.left + r)} {f(p.top + e)}H{f(p.right - c - 0.2)}L{f(p.right - e)} {f(p.top + c + 0.2)}"
            f"V{f(p.bottom - r)}A{f(r - e if r > e else 0.1)} {f(r - e if r > e else 0.1)} 0 0 1 "
            f"{f(p.right - r)} {f(p.bottom - e)}H{f(p.left + r)}"
            f"A{f(r - e if r > e else 0.1)} {f(r - e if r > e else 0.1)} 0 0 1 {f(p.left + e)} {f(p.bottom - r)}"
            f"V{f(p.top + r)}A{f(r - e if r > e else 0.1)} {f(r - e if r > e else 0.1)} 0 0 1 "
            f"{f(p.left + r)} {f(p.top + e)}Z")
    s.path(edge, stroke=PAPER_EDGE, width=1, opacity=0.9)
    fold = poly((p.right - c, p.top), (p.right - c, p.top + c - r), (p.right - c + r, p.top + c),
                (p.right, p.top + c))
    s.path(fold, fill=s.linear(p.right - c, p.top + c, p.right - c / 2, p.top + c / 2,
                               ("#DBDAE0", "#F6F6F8")))
    s.path(f"M{f(p.right - c + 0.5)} {f(p.top + 0.5)}V{f(p.top + c - r)}"
           f"A{f(r)} {f(r)} 0 0 0 {f(p.right - c + r)} {f(p.top + c - 0.5)}H{f(p.right - 0.5)}",
           stroke=PAPER_EDGE, width=1, opacity=0.9)
    if lines:
        text_lines(s, p, 3 if n <= 16 else 5)
    return s, p


def text_lines(s: Svg, p: PageBox, count: int, colour: str = "#918F9A") -> None:
    n = s.n
    gap = max(2, round(n / 10))
    x0 = p.left + max(2, round(n / 8)) - (1 if n == 16 else 0)
    x1 = p.right - max(2, round(n / 8)) + (1 if n == 16 else 0)
    y = p.top + p.fold + max(1, round(n / 16))
    w = 1 if n < 32 else round(n / 32)
    stop = p.bottom - max(2, round(n / 8))
    for i in range(count):
        if y > stop:
            break
        end = x1 if i % 3 != 2 else x0 + (x1 - x0) * 0.6
        yy = half(y) if w % 2 else snap(y)
        s.path(f"M{f(x0)} {f(yy)}H{f(end)}", stroke=colour, width=w, cap="butt")
        y += gap


def badge(s: Svg, cx: float, cy: float, size: float, colour: tuple[str, str],
          radius: float | None = None) -> None:
    """A rounded coloured square carrying a file type's mark."""
    r = radius if radius is not None else max(1.0, size / 5)
    d = rrect(cx - size / 2, cy - size / 2, size, size, r)
    if s.n >= 32:
        s.shadow(d, s.n / 64, 0.25, blur=s.n / 64)
    s.path(d, fill=s.linear(cx - size / 2, cy - size / 2, cx + size / 2, cy + size / 2, colour))


# ------------------------------------------------------------------ writer

@dataclass
class IconSet:
    """What a module draws: context -> {name: drawing(n) -> Svg}, the
    aliases (name -> another name, maybe in another context as
    "context/name"), and the drawings that need a dark version
    (name -> drawing used in Raye-Dark)."""
    icons: dict[str, dict[str, Callable[[int], Svg]]]
    aliases: dict[str, dict[str, str]] = field(default_factory=dict)
    dark: dict[str, dict[str, Callable[[int], Svg]]] = field(default_factory=dict)


def load_sets() -> list[IconSet]:
    sys.path.insert(0, HERE)
    out = []
    for m in MODULES:
        if os.path.exists(os.path.join(HERE, m + ".py")):
            out.append(importlib.import_module(m).ICONS)
    return out


def size_dirs() -> list[str]:
    return [str(n) for n in SIZES] + ["scalable"]


def px_of(size_dir: str) -> int:
    return SCALABLE if size_dir == "scalable" else int(size_dir)


def index_theme(theme: str, dirs: list[str]) -> str:
    t = THEMES[theme]
    out = ["[Icon Theme]", f"Name={t['name']}", f"Name[fr]={t['name_fr']}",
           f"Comment={t['comment']}", f"Comment[fr]={t['comment_fr']}",
           f"Inherits={t['inherits']}", "Example=folder", "FollowsColorScheme=true",
           "DesktopDefault=48", "KDE-Extensions=.svg", f"Directories={','.join(dirs)}", ""]
    for d in dirs:
        ctx, size = d.split("/")
        out.append(f"[{d}]")
        if size == "scalable":
            out += [f"Size={SCALABLE}", f"Context={CONTEXTS[ctx]}", "Type=Scalable",
                    f"MinSize={SCALABLE}", "MaxSize=512"]
        else:
            out += [f"Size={size}", f"Context={CONTEXTS[ctx]}", "Type=Fixed"]
        out.append("")
    return "\n".join(out)


def write(out: str = OUT) -> dict[str, int]:
    """Draws every set into out/Raye and links or draws out/Raye-Dark.
    Returns a count of files per context."""
    sets = load_sets()
    light, dark = os.path.join(out, "Raye"), os.path.join(out, "Raye-Dark")
    contexts = sorted({c for st in sets for c in st.icons})
    for theme_dir in (light, dark):
        for c in contexts:
            if os.path.isdir(os.path.join(theme_dir, c)):
                shutil.rmtree(os.path.join(theme_dir, c))
    counts: dict[str, int] = {}
    for st in sets:
        for ctx, icons in st.icons.items():
            for sd in size_dirs():
                ld = os.path.join(light, ctx, sd)
                dd = os.path.join(dark, ctx, sd)
                os.makedirs(ld, exist_ok=True)
                os.makedirs(dd, exist_ok=True)
                for name, draw in icons.items():
                    with open(os.path.join(ld, name + ".svg"), "w") as fh:
                        fh.write(draw(px_of(sd)).text())
                    counts[ctx] = counts.get(ctx, 0) + 1
                    dpath = os.path.join(dd, name + ".svg")
                    dark_draw = st.dark.get(ctx, {}).get(name)
                    if dark_draw:
                        with open(dpath, "w") as fh:
                            fh.write(dark_draw(px_of(sd)).text())
                    else:
                        os.symlink(f"../../../Raye/{ctx}/{sd}/{name}.svg", dpath)
        for ctx, aliases in st.aliases.items():
            for sd in size_dirs():
                for theme_dir in (light, dark):
                    for alias, target in aliases.items():
                        tctx, tname = target.split("/") if "/" in target else (ctx, target)
                        rel = f"{tname}.svg" if tctx == ctx else f"../../{tctx}/{sd}/{tname}.svg"
                        os.symlink(rel, os.path.join(theme_dir, ctx, sd, alias + ".svg"))
    for theme, theme_dir in (("Raye", light), ("Raye-Dark", dark)):
        dirs = sorted(f"{c}/{sd}" for c in contexts for sd in size_dirs()
                      if os.path.isdir(os.path.join(theme_dir, c, sd)))
        with open(os.path.join(theme_dir, "index.theme"), "w") as fh:
            fh.write(index_theme(theme, dirs))
    return counts


def main() -> int:
    counts = write()
    for ctx, k in sorted(counts.items()):
        print(f"{ctx:10s} {k // len(size_dirs())} drawings x {len(size_dirs())} sizes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
