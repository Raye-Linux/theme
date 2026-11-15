# Monochrome status icons of the Raye icon theme (theming 3.8, list 5):
# what the taskbar's quick settings, the OSDs and the tray applets show
# for the network, sound, battery, Bluetooth, notifications and updates.
# Drawn by lib.py; see there for how a drawing works.
#
# The glyphs follow Windows 11's tray: Wi-Fi as a dot and three arcs
# (the arcs that are off stay at 30 %), a speaker with sound waves, a
# battery lying down with its fill, a bell. They are drawn on a 16 unit
# grid with 1 unit lines, round joins, in the colour scheme's text
# colour: the files carry KDE's "current-color-scheme" style sheet, so
# Plasma and KDE apps recolour them with the scheme (Raye Night or Day),
# and the default colour written in the file is the one for the theme
# (dark ink in Raye, light ink in Raye-Dark) for programs that do not
# recolour. The names are the ones Plasma's applets and Breeze use.

import json
import math
import os
import re

from lib import ROOT, IconSet, Svg, circle, f, rrect

# ------------------------------------------------------------- colours


def _tokens() -> dict:
    with open(os.path.join(ROOT, "tokens", "raye-tokens.json")) as fh:
        return json.load(fh)["color"]


_C = _tokens()
# (Raye, Raye-Dark): the ink a program sees when it does not recolour
INK = {"light": _C["text.primary"]["day"], "dark": _C["text.primary"]["night"]}
NEGATIVE = {"light": _C["status.critical"]["day"], "dark": _C["status.critical"]["night"]}
NEUTRAL = {"light": _C["status.caution"]["day"], "dark": _C["status.caution"]["night"]}
POSITIVE = {"light": _C["status.success"]["day"], "dark": _C["status.success"]["night"]}
ACCENT = {"light": _C["accent.text"]["day"], "dark": _C["accent.text"]["night"]}

DIM = 0.3          # parts of a meter that are off (Wi-Fi arcs, signal bars)
GRID = 16


def style(mode: str) -> str:
    return ('<style id="current-color-scheme" type="text/css">'
            f'.ColorScheme-Text{{color:{INK[mode]}}}'
            f'.ColorScheme-NegativeText{{color:{NEGATIVE[mode]}}}'
            f'.ColorScheme-NeutralText{{color:{NEUTRAL[mode]}}}'
            f'.ColorScheme-PositiveText{{color:{POSITIVE[mode]}}}'
            f'.ColorScheme-Highlight{{color:{ACCENT[mode]}}}'
            '</style>')


# ------------------------------------------------------------- glyph model

class Glyph:
    """A drawing on the 16 unit grid. Parts are lines (stroked, 1 unit)
    and fills; each has an opacity and a colour role. A badge in the
    bottom right corner cuts a gap into the glyph under it."""

    ROLES = {"text": "ColorScheme-Text", "negative": "ColorScheme-NegativeText",
             "neutral": "ColorScheme-NeutralText", "positive": "ColorScheme-PositiveText",
             "accent": "ColorScheme-Highlight"}

    def __init__(self):
        self.parts: list[tuple[str, str, float, str, float]] = []
        self.badge_parts: list[tuple[str, str, float, str, float]] = []
        self.cut: tuple[float, float, float] | None = None
        self.shift = (0.0, 0.0)

    def line(self, d: str, opacity: float = 1.0, role: str = "text", width: float = 1.0) -> "Glyph":
        self.parts.append(("line", d, opacity, role, width))
        return self

    def fill(self, d: str, opacity: float = 1.0, role: str = "text") -> "Glyph":
        self.parts.append(("fill", d, opacity, role, 0))
        return self

    def badge(self, parts: list[tuple[str, str, str]], cx: float = 12.5, cy: float = 12.5,
              r: float = 4.0) -> "Glyph":
        """parts: (kind, d, role), drawn around (12.5, 12.5) and moved so
        they centre on (cx, cy), with a gap of radius r cut into the
        glyph around that point."""
        self.cut = (cx, cy, r)
        self.shift = (cx - 12.5, cy - 12.5)
        for kind, d, role in parts:
            self.badge_parts.append((kind, d, 1.0, role, 1.0))
        return self

    @staticmethod
    def _part(kind: str, d: str, opacity: float, role: str, width: float, gtk: bool = False) -> str:
        cls = Glyph.ROLES[role]
        op = f' opacity="{f(opacity)}"' if opacity != 1 else ""
        if gtk:
            # GTK paints these classes in its own warning/error/success colours
            extra = GTK_CLASS.get(role)
            cls = f"{cls} {extra}" if extra else cls
            if kind == "line":
                d = outline(d, width)
        if kind == "fill" or gtk:
            return f'<path d="{d}" fill="currentColor" class="{cls}"{op}/>'
        return (f'<path d="{d}" fill="none" stroke="currentColor" class="{cls}" '
                f'stroke-width="{f(width)}" stroke-linecap="round" stroke-linejoin="round"{op}/>')

    def svg(self, n: int, mode: str, gtk: bool = False) -> Svg:
        """gtk: the twin for the -symbolic name. GTK fills every rect and
        path of a symbolic icon with its own colour and drops strokes, so
        lines go out as the shape they cover, and the mask of the badge
        gap is drawn with shapes it leaves alone (Qt has no clip paths)."""
        s = Svg(n)
        k = n / GRID
        body = "".join(self._part(*p, gtk=gtk) for p in self.parts)
        defs = style(mode)
        if self.cut:
            cx, cy, r = self.cut
            marks = "".join(self._part(*p, gtk=gtk) for p in self.badge_parts)
            if self.shift != (0.0, 0.0):
                marks = f'<g transform="translate({f(self.shift[0])} {f(self.shift[1])})">{marks}</g>'
            if gtk:
                # GTK's stylesheet only recolours rect, circle and path
                shapes = (f'<polygon points="0,0 {GRID},0 {GRID},{GRID} 0,{GRID}" fill="#FFFFFF"/>'
                          f'<ellipse cx="{f(cx)}" cy="{f(cy)}" rx="{f(r)}" ry="{f(r)}" fill="#000000"/>')
            else:
                shapes = (f'<rect x="0" y="0" width="{GRID}" height="{GRID}" fill="#FFFFFF"/>'
                          f'<path d="{circle(cx, cy, r)}" fill="#000000"/>')
            defs += (f'<mask id="cut" maskUnits="userSpaceOnUse" x="0" y="0" width="{GRID}" height="{GRID}">'
                     f'{shapes}</mask>')
            body = f'<g mask="url(#cut)">{body}</g>' + marks
        s.defs.append(defs)
        s.raw(f'<g transform="scale({f(k)})">{body}</g>')
        return s


GTK_CLASS = {"negative": "error", "neutral": "warning", "positive": "success"}


# ------------------------------------------------------------- outlines
# A line of width w with round caps and joins covers the union of one
# piece per straight or circular stretch of it: a capsule, or a bent
# band with round ends. Each piece goes the same way round, so the
# nonzero rule fills their union and the holes of a closed line (a ring,
# a box) stay holes, with no boolean operations. Curves other than
# circular arcs are cut into short straight stretches first.

STEP = 0.4        # longest straight stretch of a bezier, in grid units
MAX_TURN = 120    # longest arc stretch, in degrees


TOKEN = re.compile(r"([MmLlHhVvCcSsQqTtAaZz])|(-?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?)")


def _tokens(d: str):
    if TOKEN.sub("", d).strip(" ,\t\n"):
        raise ValueError(f"not a path: {d}")
    for m in TOKEN.finditer(d):
        yield m.group(1) or float(m.group(2))


def stretches(d: str) -> list[list[tuple]]:
    """The stretches of each subpath of d: ("line", a, b) or
    ("arc", a, b, centre, radius, sweep)."""
    toks = list(_tokens(d))
    out: list[list[tuple]] = []
    cur = start = (0.0, 0.0)
    last_ctrl = None
    cmd = None
    i = 0

    def nums(k):
        nonlocal i
        v = toks[i:i + k]
        i += k
        return v

    def line(to):
        out[-1].append(("line", cur, to))

    while i < len(toks):
        if isinstance(toks[i], str):
            cmd = toks[i]
            i += 1
            if cmd in "Zz":
                if out and cur != start:
                    line(start)
                cur, last_ctrl = start, None
                continue
        rel = cmd.islower()
        c = cmd.upper()
        ox, oy = cur if rel else (0.0, 0.0)
        if c == "M":
            x, y = nums(2)
            cur = start = (x + ox, y + oy)
            out.append([])
            cmd, last_ctrl = ("l" if rel else "L"), None
            continue
        if c == "L":
            x, y = nums(2)
            to, last_ctrl = (x + ox, y + oy), None
        elif c == "H":
            (x,) = nums(1)
            to, last_ctrl = (x + ox, cur[1]), None
        elif c == "V":
            (y,) = nums(1)
            to, last_ctrl = (cur[0], y + oy), None
        elif c in "CSQT":
            if c == "C":
                x1, y1, x2, y2, x, y = nums(6)
                ctrl = [(x1 + ox, y1 + oy), (x2 + ox, y2 + oy)]
            elif c == "S":
                x2, y2, x, y = nums(4)
                ctrl = [_reflect(cur, last_ctrl), (x2 + ox, y2 + oy)]
            elif c == "Q":
                x1, y1, x, y = nums(4)
                ctrl = [(x1 + ox, y1 + oy)]
            else:
                x, y = nums(2)
                ctrl = [_reflect(cur, last_ctrl)]
            to = (x + ox, y + oy)
            prev = cur
            for p in _bezier([cur, *ctrl, to]):
                out[-1].append(("line", prev, p))
                prev = p
            cur, last_ctrl = to, ctrl[-1]
            continue
        elif c == "A":
            rx, ry, rot, large, sweep, x, y = nums(7)
            to, last_ctrl = (x + ox, y + oy), None
            out[-1].extend(_arc_stretches(cur, to, rx, ry, rot, bool(large), bool(sweep)))
            cur = to
            continue
        else:
            raise ValueError(f"path command {cmd} not handled: {d}")
        line(to)
        cur = to
    return out


def _reflect(cur, ctrl):
    return (2 * cur[0] - ctrl[0], 2 * cur[1] - ctrl[1]) if ctrl else cur


def _bezier(ctrl: list[tuple[float, float]]) -> list[tuple[float, float]]:
    length = sum(math.dist(a, b) for a, b in zip(ctrl, ctrl[1:]))
    n = max(2, math.ceil(length / STEP))
    pts = []
    for k in range(1, n + 1):
        t = k / n
        p = list(ctrl)
        while len(p) > 1:
            p = [(a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t) for a, b in zip(p, p[1:])]
        pts.append(p[0])
    return pts


def _arc_stretches(p0, p1, rx, ry, rot, large, sweep) -> list[tuple]:
    """An endpoint arc (as in the SVG spec's implementation notes) as
    circular stretches of MAX_TURN degrees at most, or as straight ones
    when it is not circular."""
    if math.dist(p0, p1) < 1e-9:
        return []
    rx, ry = abs(rx), abs(ry)
    if rx == 0 or ry == 0:
        return [("line", p0, p1)]
    phi = math.radians(rot)
    cp, sp = math.cos(phi), math.sin(phi)
    dx, dy = (p0[0] - p1[0]) / 2, (p0[1] - p1[1]) / 2
    x1, y1 = cp * dx + sp * dy, -sp * dx + cp * dy
    lam = (x1 / rx) ** 2 + (y1 / ry) ** 2
    if lam > 1:
        rx, ry = rx * math.sqrt(lam), ry * math.sqrt(lam)
    num = rx * rx * ry * ry - rx * rx * y1 * y1 - ry * ry * x1 * x1
    den = rx * rx * y1 * y1 + ry * ry * x1 * x1
    co = math.sqrt(max(0.0, num / den))
    if large == sweep:
        co = -co
    cxp, cyp = co * rx * y1 / ry, -co * ry * x1 / rx
    cx = cp * cxp - sp * cyp + (p0[0] + p1[0]) / 2
    cy = sp * cxp + cp * cyp + (p0[1] + p1[1]) / 2
    a0 = math.atan2((y1 - cyp) / ry, (x1 - cxp) / rx)
    a1 = math.atan2((-y1 - cyp) / ry, (-x1 - cxp) / rx)
    da = a1 - a0
    if sweep and da < 0:
        da += 2 * math.pi
    elif not sweep and da > 0:
        da -= 2 * math.pi
    circular = abs(rx - ry) < 1e-6
    n = math.ceil(abs(math.degrees(da)) / MAX_TURN) if circular else \
        max(2, math.ceil(abs(da) * max(rx, ry) / STEP))
    pts = [p0]
    for k in range(1, n + 1):
        a = a0 + da * k / n
        ex, ey = rx * math.cos(a), ry * math.sin(a)
        pts.append((cx + cp * ex - sp * ey, cy + sp * ex + cp * ey) if k < n else p1)
    if not circular:
        return [("line", a, b) for a, b in zip(pts, pts[1:])]
    return [("arc", a, b, (cx, cy), rx, sweep) for a, b in zip(pts, pts[1:])]


def _cap_sweep(at, centre, forward) -> int:
    """Sweep flag of the half circle around centre that starts at at and
    bulges towards forward."""
    vx, vy = at[0] - centre[0], at[1] - centre[1]
    return 1 if forward[0] * -vy + forward[1] * vx > 0 else 0


def _pieces(st: tuple, r: float) -> list[list]:
    """The closed pieces a stretch covers, each a list of ("M", p),
    ("L", p), ("A", radius, sweep, p)."""
    if st[0] == "arc" and st[4] <= r:
        # too tight for a band: short straight stretches instead
        pts = _arc_points(st, 8)
        return [_piece(("line", a, b), r) for a, b in zip(pts, pts[1:])]
    return [_piece(st, r)]


def _piece(st: tuple, r: float) -> list:
    if st[0] == "line":
        a, b = st[1], st[2]
        dx, dy = b[0] - a[0], b[1] - a[1]
        n = math.hypot(dx, dy)
        if n < 1e-9:
            x, y = a
            return [("M", (x, y - r)), ("A", r, 1, (x, y + r)), ("A", r, 1, (x, y - r))]
        t = (dx / n, dy / n)
        px, py = -t[1] * r, t[0] * r
        s0, s1 = (a[0] + px, a[1] + py), (b[0] + px, b[1] + py)
        e0, e1 = (a[0] - px, a[1] - py), (b[0] - px, b[1] - py)
        return [("M", s0), ("L", s1), ("A", r, _cap_sweep(s1, b, t), e1),
                ("L", e0), ("A", r, _cap_sweep(e0, a, (-t[0], -t[1])), s0)]
    _, a, b, c, big, sweep = st

    def at(p, rad):
        k = rad / big
        return (c[0] + (p[0] - c[0]) * k, c[1] + (p[1] - c[1]) * k)

    # forward at an end of the arc: perpendicular to the radius, the way it turns
    def ahead(p, sw):
        vx, vy = (p[0] - c[0]) / big, (p[1] - c[1]) / big
        return (-vy, vx) if sw else (vy, -vx)

    o0, o1, i0, i1 = at(a, big + r), at(b, big + r), at(a, big - r), at(b, big - r)
    back = ahead(a, sweep)
    return [("M", o0), ("A", big + r, int(sweep), o1),
            ("A", r, _cap_sweep(o1, b, ahead(b, sweep)), i1),
            ("A", big - r, int(not sweep), i0),
            ("A", r, _cap_sweep(i0, a, (-back[0], -back[1])), o0)]


def _arc_points(st: tuple, k: int) -> list[tuple[float, float]]:
    _, a, b, c, rad, sw = st
    t0 = math.atan2(a[1] - c[1], a[0] - c[0])
    dt = math.atan2(b[1] - c[1], b[0] - c[0]) - t0
    if sw and dt < 0:
        dt += 2 * math.pi
    if not sw and dt > 0:
        dt -= 2 * math.pi
    return [(c[0] + rad * math.cos(t0 + dt * i / k), c[1] + rad * math.sin(t0 + dt * i / k))
            for i in range(k + 1)]


def _area(piece: list) -> float:
    fine = _polygon(piece)
    return sum(p[0] * q[1] - q[0] * p[1] for p, q in zip(fine, fine[1:] + fine[:1])) / 2


def _reverse(piece: list) -> list:
    out = [("M", piece[-1][-1])]
    for prev, op in zip(reversed(piece[:-1]), reversed(piece[1:])):
        end = prev[-1]
        out.append(("L", end) if op[0] == "L" else ("A", op[1], 1 - op[2], end))
    return out


def _text(piece: list) -> str:
    out = []
    for op in piece:
        x, y = op[-1]
        if op[0] == "A":
            out.append(f"A{f(op[1])} {f(op[1])} 0 0 {op[2]} {f(x)} {f(y)}")
        else:
            out.append(f"{op[0]}{f(x)} {f(y)}")
    return "".join(out) + "Z"


def _polygon(piece: list) -> list[tuple[float, float]]:
    fine = []
    for sub in stretches(_text(piece)):
        for st in sub:
            fine += _arc_points(st, 8)[:-1] if st[0] == "arc" else [st[1]]
    return fine


def outline(d: str, width: float = 1.0) -> str:
    """The filled shape a round-capped, round-joined line along d covers."""
    r = width / 2
    out = []
    for sub in stretches(d):
        for p in [p for st in sub for p in _pieces(st, r)]:
            if _area(p) < 0:
                p = _reverse(p)
            out.append(_text(p))
    return "".join(out)


def drawing(build, gtk: bool = False):
    """Turns a function returning a Glyph into the (light, dark) pair of
    drawings lib.py writes."""
    return (lambda n: build().svg(n, "light", gtk), lambda n: build().svg(n, "dark", gtk))


def with_twins(drawn: dict, aliases: dict) -> tuple[dict, dict]:
    """(pairs, aliases) for lib.py: every drawing and alias also answers
    to its -symbolic name. A drawing with lines or a badge gap gets its
    own -symbolic file drawn for GTK (see Glyph.svg); the others are the
    same picture either way and get a link."""
    pairs, out = {}, {}
    for name, build in drawn.items():
        pairs[name] = drawing(build)
        if build().svg(16, "light", gtk=True).text() != build().svg(16, "light").text():
            pairs[name + "-symbolic"] = drawing(build, gtk=True)
        else:
            out[name + "-symbolic"] = name

    def twin(target: str) -> str:
        return target + "-symbolic" if target + "-symbolic" in pairs else target
    for alias, target in aliases.items():
        if alias.endswith("-symbolic"):
            out[alias] = twin(target)
    for alias, target in aliases.items():
        if not alias.endswith("-symbolic"):
            out[alias] = target
            out.setdefault(alias + "-symbolic", twin(target))
    return pairs, out


# ------------------------------------------------------------- badges
# Small marks in the bottom right corner, centred on (12.5, 12.5).

def badge_cross(role: str = "text"):
    return [("line", "M10.75 10.75L14.25 14.25M14.25 10.75L10.75 14.25", role)]


def badge_dots():
    return [("fill", circle(10.3, 13.5, 0.8), "text"), ("fill", circle(12.5, 13.5, 0.8), "text"),
            ("fill", circle(14.7, 13.5, 0.8), "text")]


def badge_alert(role: str = "neutral"):
    return [("line", "M12.5 10V12.6", role), ("fill", circle(12.5, 14.6, 0.75), role)]


def badge_lock():
    return [("fill", rrect(10, 12, 5, 3.5, 0.75), "text"),
            ("line", "M11.25 12V11A1.25 1.25 0 0 1 13.75 11V12", "text")]


def badge_dot(role: str):
    return [("fill", circle(12.5, 12.5, 2.25), role)]


SLASH = "M2.5 2.5L13.5 13.5"

# ------------------------------------------------------------- Wi-Fi

WIFI_DOT = (8, 12.5)
WIFI_RADII = (3.25, 6.25, 9.25)


def _arc(r: float) -> str:
    import math
    cx, cy = WIFI_DOT
    a = math.radians(45)
    x0, y0 = cx - r * math.sin(a), cy - r * math.cos(a)
    x1 = cx + r * math.sin(a)
    return f"M{f(x0)} {f(y0)}A{f(r)} {f(r)} 0 0 1 {f(x1)} {f(y0)}"


def wifi(arcs_on: int, badge=None, all_dim: bool = False) -> Glyph:
    g = Glyph()
    g.fill(circle(*WIFI_DOT, 1.25), DIM if all_dim else 1.0)
    for i, r in enumerate(WIFI_RADII):
        g.line(_arc(r), 1.0 if (i < arcs_on and not all_dim) else DIM)
    if badge:
        # smaller and further out, so the dot stays whole
        g.badge(badge, cx=13, cy=13, r=3.25)
    return g


def hotspot() -> Glyph:
    g = Glyph()
    g.fill(circle(8, 8, 1.25))
    g.line("M5.2 5.2A4 4 0 0 0 5.2 10.8M10.8 5.2A4 4 0 0 1 10.8 10.8")
    g.line("M3.05 3.05A7 7 0 0 0 3.05 12.95M12.95 3.05A7 7 0 0 1 12.95 12.95")
    return g


# ------------------------------------------------------------- wired

def wired(badge=None) -> Glyph:
    # a network cable plug, latch up, the cable leaving at its foot: the
    # screen drawn here before read as "connected to a display". Same plug
    # as Raye Setup's network page.
    g = Glyph()
    g.line("M4 4H6.5V2.5H9.5V4H12V9.5H4Z")
    g.line("M6.5 6V7.5M8 6V7.5M9.5 6V7.5")
    g.line("M6 9.5L6.5 11.5H9.5L10 9.5M8 11.5V14")
    if badge:
        g.badge(badge)
    return g


# ------------------------------------------------------------- mobile

def mobile(bars_on: int, badge=None) -> Glyph:
    g = Glyph()
    for i in range(4):
        x = 3 + i * 3
        top = 12.5 - (i + 1) * 2.5
        g.fill(rrect(x - 0.75, top, 1.5, 13.5 - top, 0.5), 1.0 if i < bars_on else DIM)
    if badge:
        g.badge(badge)
    return g


# ------------------------------------------------------------- VPN, offline, airplane

def shield(badge=None) -> Glyph:
    g = Glyph()
    g.line("M8 1.75L13 3.75V7.75C13 10.75 10.9 12.9 8 14.25C5.1 12.9 3 10.75 3 7.75V3.75Z")
    g.fill(circle(8, 7, 1.4))
    g.line("M8 8V10.5")
    if badge:
        g.badge(badge)
    return g


def globe(badge=None) -> Glyph:
    g = Glyph()
    g.line(circle(8, 8, 5.75))
    g.line("M2.25 8H13.75M8 2.25C5.9 4.6 5.9 11.4 8 13.75C10.1 11.4 10.1 4.6 8 2.25")
    if badge:
        g.badge(badge)
    return g


PLANE = ("M8 1.5C8.7 1.5 9 2.3 9 3V6.2L14 9.2V10.8L9 9.3V12L10.75 13.25V14.5L8 13.75"
         "L5.25 14.5V13.25L7 12V9.3L2 10.8V9.2L7 6.2V3C7 2.3 7.3 1.5 8 1.5Z")


def airplane(on: bool) -> Glyph:
    g = Glyph()
    if on:
        g.fill(PLANE)
    else:
        g.line(PLANE)
        g.line(SLASH)
    return g


# ------------------------------------------------------------- sound

SPEAKER = "M2.5 6H5L8.5 3V13L5 10H2.5Z"
WAVES = ("M10.5 6.25A2.5 2.5 0 0 1 10.5 9.75",
         "M12 4.5A5 5 0 0 1 12 11.5",
         "M13.5 2.75A7.5 7.5 0 0 1 13.5 13.25")


def speaker(waves: int, muted: bool = False) -> Glyph:
    g = Glyph()
    g.line(SPEAKER)
    if muted:
        g.line("M11 6.25L14.5 9.75M14.5 6.25L11 9.75")
    else:
        for i in range(waves):
            g.line(WAVES[i])
    return g


MIC = rrect(6, 1.75, 4, 7.5, 2)


def microphone(muted: bool = False) -> Glyph:
    g = Glyph()
    g.line(MIC)
    g.line("M3.75 7.5A4.25 4.25 0 0 0 12.25 7.5M8 11.75V14.25M5.75 14.25H10.25")
    if muted:
        g.line(SLASH, role="text")
    return g


# ------------------------------------------------------------- battery
# Lying down like Windows': body 11 x 7, terminal on the right.

BODY = (1.5, 4.5, 11.5, 7)


def battery(level: int, charging: bool = False, saver: bool = False) -> Glyph:
    g = Glyph()
    x, y, w, h = BODY
    g.line(rrect(x, y, w, h, 1.75))
    g.fill(rrect(x + w + 0.75, y + 2.25, 1.25, h - 4.5, 0.6))
    inner = w - 3
    fill_w = round(inner * level / 100 * 4) / 4
    low = level <= 10 and not charging
    if level > 0 and fill_w > 0:
        g.fill(rrect(x + 1.5, y + 1.5, max(fill_w, 1), h - 3, 0.6),
               role="negative" if low else "text")
    if charging:
        g.badge([("fill", "M13.5 8.5L10.25 13H12.4L11.5 16L14.75 11.5H12.6Z", "text")],
                cx=12.5, cy=12.5, r=3.6)
    elif saver:
        g.badge([("fill", "M10 15C10 11.8 12.2 10.1 15.25 9.75C15.25 13 13.4 15 10.6 15.1Z", "positive"),
                 ("line", "M10.25 15.25L12.75 12.5", "text")], cx=12.5, cy=12.5, r=3.75)
    elif level == 0:
        g.badge(badge_cross("negative"), cx=12.5, cy=12.5, r=3.5)
    return g


def battery_missing() -> Glyph:
    g = Glyph()
    x, y, w, h = BODY
    g.line(rrect(x, y, w, h, 1.75), DIM)
    g.fill(rrect(x + w + 0.75, y + 2.25, 1.25, h - 4.5, 0.6), DIM)
    g.badge(badge_alert("neutral"))
    return g


def leaf() -> Glyph:
    g = Glyph()
    g.line("M3 13C3 7.5 7 3.5 13 3C13 9 9 13 3.5 13Z")
    g.line("M3.5 12.5L9 7")
    return g


# ------------------------------------------------------------- Bluetooth

RUNE = "M5 5L11 10.75L8 13.5V2.5L11 5.25L5 11"


def bluetooth(state: str) -> Glyph:
    g = Glyph()
    if state == "off":
        g.line(RUNE, DIM)
        g.line(SLASH)
    else:
        g.line(RUNE)
        if state == "connected":
            g.fill(circle(2.75, 8, 0.9))
            g.fill(circle(13.25, 8, 0.9))
    return g


# ------------------------------------------------------------- bell

BELL = ("M8 2C5.6 2 4.25 3.9 4.25 6.25V9L3 11.25H13L11.75 9V6.25C11.75 3.9 10.4 2 8 2Z")


def bell(state: str = "plain") -> Glyph:
    g = Glyph()
    g.line(BELL)
    g.line("M6.5 13.25A1.6 1.6 0 0 0 9.5 13.25")
    if state == "new":
        g.badge(badge_dot("accent"), cx=12.25, cy=3.75, r=3.25)
    elif state == "off":
        g.line(SLASH)
    return g


# ------------------------------------------------------------- updates

def _sync_arrow(start: float, end: float, r: float = 5.0) -> str:
    """An arc of the circle (8, 8, r) from angle start to end (degrees,
    clockwise on screen) with an arrowhead at its end."""
    import math
    a0, a1 = math.radians(start), math.radians(end)
    x0, y0 = 8 + r * math.cos(a0), 8 + r * math.sin(a0)
    x1, y1 = 8 + r * math.cos(a1), 8 + r * math.sin(a1)
    large = 1 if (end - start) % 360 > 180 else 0
    d = f"M{f(x0)} {f(y0)}A{f(r)} {f(r)} 0 {large} 1 {f(x1)} {f(y1)}"
    # heading at the end, then two short strokes back from the tip
    hx, hy = -math.sin(a1), math.cos(a1)
    for turn in (-40, 40):
        t = math.radians(180 + turn)
        bx = hx * math.cos(t) - hy * math.sin(t)
        by = hx * math.sin(t) + hy * math.cos(t)
        d += f"M{f(x1)} {f(y1)}L{f(x1 + 2.4 * bx)} {f(y1 + 2.4 * by)}"
    return d


def updates(role: str | None) -> Glyph:
    g = Glyph()
    g.line(_sync_arrow(190, 330))
    g.line(_sync_arrow(10, 150))
    if role:
        g.badge(badge_dot(role), cx=12.5, cy=12.5, r=3.5)
    return g


# ------------------------------------------------------------- display

def sun(rays_long: bool = True) -> Glyph:
    import math
    g = Glyph()
    g.line(circle(8, 8, 2.75))
    r0, r1 = 4.75, 6.5 if rays_long else 5.75
    parts = []
    for i in range(8):
        a = math.radians(i * 45)
        parts.append(f"M{f(8 + r0 * math.cos(a))} {f(8 + r0 * math.sin(a))}"
                     f"L{f(8 + r1 * math.cos(a))} {f(8 + r1 * math.sin(a))}")
    g.line("".join(parts))
    return g


MOON = "M12.75 9.75A5.5 5.5 0 1 1 6.25 3.25A4.5 4.5 0 0 0 12.75 9.75Z"


def moon(on: bool) -> Glyph:
    g = Glyph()
    if on:
        g.fill(MOON)
    else:
        g.line(MOON)
    return g


# ------------------------------------------------------------- the set

STATUS: dict[str, object] = {}
for lvl, arcs in (("00", 0), ("20", 1), ("40", 1), ("60", 2), ("80", 3), ("100", 3)):
    STATUS[f"network-wireless-connected-{lvl}"] = (lambda a: lambda: wifi(a))(arcs)
    STATUS[f"network-wireless-{lvl}-locked"] = (lambda a: lambda: wifi(a, badge_lock()))(arcs)
    STATUS[f"network-wireless-{lvl}-limited"] = (lambda a: lambda: wifi(a, badge_alert()))(arcs)
STATUS.update({
    "network-wireless-disconnected": lambda: wifi(0, badge_cross(), all_dim=True),
    "network-wireless-acquiring": lambda: wifi(0, badge_dots(), all_dim=True),
    "network-wireless-off": lambda: wifi(0, badge_cross(), all_dim=True),
    "network-wireless-hotspot": hotspot,
    "network-wired-activated": wired,
    "network-wired-disconnected": lambda: wired(badge_cross()),
    "network-wired-acquiring": lambda: wired(badge_dots()),
    "network-wired-limited": lambda: wired(badge_alert()),
    "network-vpn": shield,
    "network-vpn-acquiring": lambda: shield(badge_dots()),
    "network-offline": lambda: globe(badge_cross()),
    "network-limited": lambda: globe(badge_alert()),
    "network-flightmode-on": lambda: airplane(True),
    "network-flightmode-off": lambda: airplane(False),
    "network-mobile-off": lambda: mobile(0, badge_cross()),
    "audio-volume-muted": lambda: speaker(0, muted=True),
    "audio-volume-low": lambda: speaker(1),
    "audio-volume-medium": lambda: speaker(2),
    "audio-volume-high": lambda: speaker(3),
    "audio-input-microphone": microphone,
    "microphone-sensitivity-muted": lambda: microphone(True),
    "battery-missing": battery_missing,
    "battery-profile-powersave": leaf,
    "network-bluetooth": lambda: bluetooth("on"),
    "network-bluetooth-activated": lambda: bluetooth("connected"),
    "network-bluetooth-inactive": lambda: bluetooth("off"),
    "notification-inactive": lambda: bell("plain"),
    "notification-active": lambda: bell("new"),
    "notifications-disabled": lambda: bell("off"),
    "update-none": lambda: updates(None),
    "update-low": lambda: updates("accent"),
    "update-medium": lambda: updates("neutral"),
    "update-high": lambda: updates("negative"),
    "brightness-high": lambda: sun(True),
    "brightness-low": lambda: sun(False),
    "redshift-status-on": lambda: moon(True),
    "redshift-status-off": lambda: moon(False),
})
for lvl in range(0, 101, 20):
    STATUS[f"network-mobile-{lvl}"] = (lambda b: lambda: mobile(b))(round(lvl / 25))
for lvl in range(0, 101, 10):
    STATUS[f"battery-{lvl:03d}"] = (lambda v: lambda: battery(v))(lvl)
    STATUS[f"battery-{lvl:03d}-charging"] = (lambda v: lambda: battery(v, charging=True))(lvl)
    STATUS[f"battery-{lvl:03d}-saver"] = (lambda v: lambda: battery(v, saver=True))(lvl)

# Other names applets and programs ask for, pointing at a drawing.
ALIASES = {
    "network-wireless-connected-25": "network-wireless-connected-20",
    "network-wireless-connected-50": "network-wireless-connected-40",
    "network-wireless-connected-75": "network-wireless-connected-60",
    "network-wireless-0": "network-wireless-connected-00",
    "network-wireless-20": "network-wireless-connected-20",
    "network-wireless-40": "network-wireless-connected-40",
    "network-wireless-60": "network-wireless-connected-60",
    "network-wireless-80": "network-wireless-connected-80",
    "network-wireless-100": "network-wireless-connected-100",
    "network-wireless-signal-none": "network-wireless-connected-00",
    "network-wireless-signal-weak": "network-wireless-connected-20",
    "network-wireless-signal-ok": "network-wireless-connected-60",
    "network-wireless-signal-good": "network-wireless-connected-80",
    "network-wireless-signal-excellent": "network-wireless-connected-100",
    "network-wireless": "network-wireless-connected-100",
    "network-wireless-on": "network-wireless-connected-100",
    "network-wired": "network-wired-activated",
    "network-wired-unavailable": "network-wired-disconnected",
    "network-wired-no-route": "network-wired-limited",
    "network-unavailable": "network-offline",
    "network-disconnect": "network-offline",
    "network-mobile-on": "network-mobile-100",
    "airplane-mode": "network-flightmode-on",
    "airplane-mode-disabled": "network-flightmode-off",
    "audio-volume-off": "audio-volume-muted",
    "audio-on": "audio-volume-high",
    "audio-ready": "audio-volume-high",
    "audio-volume-high-danger": "audio-volume-high",
    "audio-input-microphone-muted": "microphone-sensitivity-muted",
    "microphone-sensitivity-low": "audio-input-microphone",
    "microphone-sensitivity-medium": "audio-input-microphone",
    "microphone-sensitivity-high": "audio-input-microphone",
    "microphone-disabled-symbolic": "microphone-sensitivity-muted",
    "battery-empty": "battery-000",
    "battery-caution": "battery-010",
    "battery-low": "battery-020",
    "battery-good": "battery-060",
    "battery-full": "battery-100",
    "battery-empty-charging": "battery-000-charging",
    "battery-caution-charging": "battery-010-charging",
    "battery-low-charging": "battery-020-charging",
    "battery-good-charging": "battery-060-charging",
    "battery-full-charging": "battery-100-charging",
    "battery-full-charged": "battery-100-charging",
    "battery-ac-adapter": "battery-100-charging",
    "battery-level-100-charged": "battery-100-charging",
    "bluetooth": "network-bluetooth",
    "bluetooth-active": "network-bluetooth-activated",
    "bluetooth-disabled": "network-bluetooth-inactive",
    "bluetooth-online": "network-bluetooth",
    "bluetooth-offline": "network-bluetooth-inactive",
    "notifications": "notification-inactive",
    "notification-disabled": "notifications-disabled",
    "notification-new": "notification-active",
    "notifications-new": "notification-active",
    "software-update-available": "update-low",
    "software-update-urgent": "update-high",
    "update": "update-none",
    "updated": "update-none",
    "video-display-brightness": "brightness-high",
    "display-brightness": "brightness-high",
    "redshift-status-day": "brightness-high",
}
_PAIRS, ALIASES = with_twins(STATUS, ALIASES)

ICONS = IconSet(
    icons={"status": {name: pair[0] for name, pair in _PAIRS.items()}},
    aliases={"status": ALIASES},
    dark={"status": {name: pair[1] for name, pair in _PAIRS.items()}},
)
