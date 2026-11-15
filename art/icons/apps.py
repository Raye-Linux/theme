# Applications and menu categories of the Raye icon theme (theming 3.8,
# lists 3 and 4). Drawn by lib.py; see there for how a drawing works.
#
# One family: every app sits on the Raye tile, a rounded square with the
# same size, corners and light, and a white symbol. Raye's own apps
# (Settings, Store, Update, Install Raye) and the menu categories use the
# violet to magenta tile; the everyday apps (Files, Terminal, Text Editor,
# Photos...) have a tile in a colour of their own, so they are told apart
# at a glance in Start and on the taskbar.
# Third-party brands (Firefox, Steam...) keep their own icons: nothing
# here uses their names.

import math

from lib import GLOW, VIOLET, IconSet, Svg, circle, poly, rrect

WHITE = "#FFFFFF"


def sheen(s: Svg, d: str, top: float, bottom: float) -> None:
    """The light from the top left: white fading out over the top half."""
    s.path(d, fill=s.linear(0, top, 0, top + (bottom - top) * 0.55,
                            [(0, WHITE, 0.28), (1, WHITE, 0)]))


def drop(s: Svg, d: str) -> None:
    n = s.n
    if n >= 32:
        s.shadow(d, n / 40, 0.3, blur=n / 48)
    elif n >= 22:
        s.shadow(d, 1, 0.25)


def white_line(n: int) -> float:
    return {16: 1.25, 22: 1.5, 24: 1.75, 32: 2.25, 48: 3.0}.get(n, n / 16)


# ------------------------------------------------------------- the Raye tile

def tile(n: int, colours: tuple[str, ...] = (VIOLET, GLOW)) -> tuple[Svg, float, float]:
    """The rounded tile. Returns the drawing, the tile's inset and size."""
    s = Svg(n)
    m = max(1, round(n * 0.0625))
    size = n - 2 * m
    d = rrect(m, m, size, size, size * 0.24)
    drop(s, d)
    s.path(d, fill=s.linear(m, m, n - m, n - m, colours))
    sheen(s, d, m, n - m)
    if n >= 22:
        s.path(rrect(m + 0.5, m + 0.5, size - 1, size - 1, size * 0.24 - 0.5),
               stroke=WHITE, width=1, opacity=0.18)
    return s, m, size


class Grid:
    """Paths written on a 24 unit box drawn inside the tile."""

    def __init__(self, s: Svg, m: float, size: float, box: float = 0.62):
        self.s = s
        self.k = size * box / 24
        self.x0 = m + size * (1 - box) / 2
        self.y0 = m + size * (1 - box) / 2

    def _g(self, inner: str) -> None:
        self.s.raw(f'<g transform="translate({self.x0:.3f} {self.y0:.3f}) scale({self.k:.4f})">'
                   + inner + "</g>")

    def line(self, d: str, colour: str = WHITE, opacity: float = 1.0) -> None:
        w = white_line(self.s.n) / self.k
        op = f' opacity="{opacity}"' if opacity != 1 else ""
        self._g(f'<path d="{d}" fill="none" stroke="{colour}" stroke-width="{w:.3f}" '
                f'stroke-linecap="round" stroke-linejoin="round"{op}/>')

    def fill(self, d: str, colour: str = WHITE, opacity: float = 1.0, rule: str = "") -> None:
        op = f' opacity="{opacity}"' if opacity != 1 else ""
        fr = ' fill-rule="evenodd"' if rule else ""
        self._g(f'<path d="{d}" fill="{colour}"{op}{fr}/>')


def eye(g: Grid, cx: float, cy: float, w: float, colour: str = VIOLET, pupil: str = WHITE) -> None:
    """The Raye eye on a grid: an almond with a round pupil."""
    h = w * 0.62
    g.fill(f"M{cx - w / 2} {cy}Q{cx} {cy - h} {cx + w / 2} {cy}Q{cx} {cy + h} {cx - w / 2} {cy}Z", colour)
    g.fill(circle(cx, cy, w * 0.17), pupil)


# ------------------------------------------------------------- Raye apps

def gear_path(cx: float, cy: float, r_out: float, r_in: float, teeth: int, hole: float) -> str:
    pts = []
    for i in range(teeth * 4):
        a = 2 * math.pi * i / (teeth * 4) - math.pi / 2
        r = r_out if i % 4 in (1, 2) else r_in
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return poly(*pts) + circle(cx, cy, hole)


def settings(n: int) -> Svg:
    """Raye Settings: a white gear on the tile, the eye in its middle."""
    s, m, size = tile(n)
    g = Grid(s, m, size, 0.74)
    teeth = 8 if n > 16 else 6
    g.fill(gear_path(12, 12, 11, 8.6, teeth, 5.4), rule="evenodd")
    if n >= 22:
        eye(g, 12, 12, 7.5)
    else:
        g.fill(circle(12, 12, 2.6))
    return s


def store(n: int) -> Svg:
    """Raye Store: a shopping bag with the eye."""
    s, m, size = tile(n)
    g = Grid(s, m, size, 0.66)
    g.fill(rrect(3, 7, 18, 16, 2.5))
    g.line("M8 9V6A4 4 0 0 1 16 6V9", WHITE)
    if n >= 22:
        eye(g, 12, 15, 9)
    else:
        g.fill(circle(12, 15, 2.6), VIOLET)
    return s


def update(n: int) -> Svg:
    """Raye Update: two arrows chasing each other around the eye."""
    s, m, size = tile(n)
    g = Grid(s, m, size, 0.66)
    r = 9

    def arrow(a0: float, a1: float) -> None:
        p0 = (12 + r * math.cos(math.radians(a0)), 12 + r * math.sin(math.radians(a0)))
        p1 = (12 + r * math.cos(math.radians(a1)), 12 + r * math.sin(math.radians(a1)))
        g.line(f"M{p0[0]:.2f} {p0[1]:.2f}A{r} {r} 0 0 1 {p1[0]:.2f} {p1[1]:.2f}")
        hx, hy = -math.sin(math.radians(a1)), math.cos(math.radians(a1))
        tip = (p1[0] + hx * 2.2, p1[1] + hy * 2.2)
        nx, ny = -hy, hx
        g.fill(poly(tip, (p1[0] + nx * 2.6, p1[1] + ny * 2.6), (p1[0] - nx * 2.6, p1[1] - ny * 2.6)))

    arrow(200, 320)
    arrow(20, 140)
    if n >= 22:
        eye(g, 12, 12, 8, WHITE, VIOLET)
    else:
        g.fill(circle(12, 12, 2.2))
    return s


def installer(n: int) -> Svg:
    """Install Raye: the eye above a drive, an arrow down into it."""
    s, m, size = tile(n)
    g = Grid(s, m, size, 0.68)
    g.fill(rrect(2, 15, 20, 7, 2))
    g.fill(circle(18, 18.5, 1.2), VIOLET)
    g.line("M12 2V11.5M8 8L12 12L16 8")
    if n >= 22:
        g.fill(rrect(5, 17.75, 7, 1.5, 0.75), VIOLET, 0.5)
    return s


def windows_apps(n: int) -> Svg:
    """Windows programs: a window frame on the tile with the eye inside.
    Never anybody's logo."""
    s, m, size = tile(n, ("#5B22D6", VIOLET, GLOW))
    g = Grid(s, m, size, 0.66)
    g.fill(rrect(1, 3, 22, 18, 2.5))
    g.fill("M1 8H23V6A3 3 0 0 0 20 3H4A3 3 0 0 0 1 6Z", "#E5D8FF")
    if n >= 22:
        g.fill(circle(19.5, 5.5, 0.9), VIOLET)
        g.fill(circle(16.5, 5.5, 0.9), VIOLET, 0.5)
        eye(g, 12, 14.5, 10)
    else:
        g.fill(circle(12, 14.5, 3), VIOLET)
    return s


def app_chooser(n: int) -> Svg:
    """Open with: four apps on the tile, the one being picked in full."""
    s, m, size = tile(n)
    g = Grid(s, m, size, 0.6 if n >= 22 else 0.7)
    r = 2.5 if n >= 22 else 3
    g.fill(rrect(1, 1, 10, 10, r))
    for x, y in ((13, 1), (1, 13), (13, 13)):
        g.fill(rrect(x, y, 10, 10, r), opacity=0.55)
    return s


# ------------------------------------------------------------- everyday apps
# One drawing each: the tile in the app's own colour, a white symbol on a
# 24 unit grid. Details on the symbol are cut from it in the tile's colour
# (`ink`), and what does not read at 16 px is left out.

TONES = {
    "violet": ("#6A2EEB", "#A97BFF"),
    "indigo": ("#3D2BD6", "#7B6BFF"),
    "blue": ("#1F5FE0", "#62A4FF"),
    "teal": ("#0B8A8F", "#35CDBA"),
    "green": ("#118A5C", "#46D092"),
    "amber": ("#D9760A", "#FFB640"),
    "rose": ("#D21F62", "#FF6A8E"),
    "magenta": ("#A626D4", "#F05CC0"),
    "graphite": ("#302F35", "#5E5C66"),
}
TONE_OF: dict[str, str] = {}


def app(tone: str, draw, box: float = 0.66):
    """A drawing for the set: tile of that tone, draw(grid, n, ink)."""
    colours = TONES[tone]

    def make(n: int) -> Svg:
        s, m, size = tile(n, colours)
        draw(Grid(s, m, size, box if n > 16 else min(0.78, box + 0.1)), n, colours[0])
        return s
    return make


def _files(g: Grid, n: int, ink: str) -> None:
    g.fill("M2 6.5A2.5 2.5 0 0 1 4.5 4H9.2L11.7 6.6H19.5A2.5 2.5 0 0 1 22 9.1V18"
           "A2.5 2.5 0 0 1 19.5 20.5H4.5A2.5 2.5 0 0 1 2 18Z")
    if n >= 22:
        g.line("M2.5 10.2H21.5", ink, 0.45)


def _terminal(g: Grid, n: int, ink: str) -> None:
    g.line("M3.5 6L10 12L3.5 18")
    g.line("M12.5 18.5H20.5")


def _text_editor(g: Grid, n: int, ink: str) -> None:
    if n > 16:
        g.line("M3 5H21M3 10.5H14M3 16H9")
    else:
        g.line("M3 5H21M3 11H12")
    g.fill("M20.2 9.6L22.9 12.3L14.3 20.9L10.8 21.7L11.6 18.2Z")


def _calculator(g: Grid, n: int, ink: str) -> None:
    g.fill(rrect(3.5, 1.5, 17, 21, 3))
    g.fill(rrect(6.5, 4.5, 11, 4, 1), ink)
    keys = (7.5, 12, 16.5) if n > 16 else (8, 16)
    rows = (13, 18) if n > 16 else (15,)
    for y in rows:
        for x in keys:
            g.fill(circle(x, y, 1.45 if n > 16 else 2), ink)


def _screenshot(g: Grid, n: int, ink: str) -> None:
    g.line("M2.5 8V5A2.5 2.5 0 0 1 5 2.5H8M16 2.5H19A2.5 2.5 0 0 1 21.5 5V8"
           "M21.5 16V19A2.5 2.5 0 0 1 19 21.5H16M8 21.5H5A2.5 2.5 0 0 1 2.5 19V16")
    g.fill(circle(12, 12, 4.4))


def _photos(g: Grid, n: int, ink: str) -> None:
    g.fill(rrect(1.5, 3.5, 21, 17, 3))
    g.fill("M4 18L9.5 10.5L13.5 15.5L16 12.8L20 18Z", ink)
    if n > 16:
        g.fill(circle(16.8, 8, 1.9), ink)


def _document_viewer(g: Grid, n: int, ink: str) -> None:
    g.fill("M5.5 1.5H14L20 7.5V20A2.5 2.5 0 0 1 17.5 22.5H5.5A2.5 2.5 0 0 1 3 20V4A2.5 2.5 0 0 1 5.5 1.5Z")
    g.fill("M14 1.5V5.5A2 2 0 0 0 16 7.5H20Z", ink, 0.5)
    if n > 16:
        g.line("M7 12H16M7 16.5H16", ink)
    else:
        g.line("M7 14H16", ink)


def _media_player(g: Grid, n: int, ink: str) -> None:
    g.fill("M6.5 4.3C6.5 2.8 8.1 1.9 9.4 2.7L21.2 10.3C22.4 11.1 22.4 12.9 21.2 13.7"
           "L9.4 21.3C8.1 22.1 6.5 21.2 6.5 19.7Z")


def _music(g: Grid, n: int, ink: str) -> None:
    g.line("M9 18V5.5L20 3V15.5")
    g.fill(circle(6.2, 18.2, 3.2))
    g.fill(circle(17.2, 15.7, 3.2))


def _archive(g: Grid, n: int, ink: str) -> None:
    g.fill(rrect(1.5, 3, 21, 5.5, 1.8))
    g.fill("M3.5 10H20.5V18.5A2.5 2.5 0 0 1 18 21H6A2.5 2.5 0 0 1 3.5 18.5Z")
    g.line("M9.5 14H14.5", ink)


def _disks(g: Grid, n: int, ink: str) -> None:
    g.fill(circle(12, 12, 10.5))
    g.fill("M12 12V3.5A8.5 8.5 0 0 1 20.5 12Z", ink)
    if n > 16:
        g.fill(circle(12, 12, 1.9), ink)


def _task_manager(g: Grid, n: int, ink: str) -> None:
    g.line("M1.5 13H6.5L9.5 4.5L14.5 19.5L17.5 11H22.5")


def _help(g: Grid, n: int, ink: str) -> None:
    g.line("M6.5 8.5A5.5 5.5 0 1 1 15 13C13 14.2 12 15 12 17")
    g.fill(circle(12, 21, 1.7))


def _passwords(g: Grid, n: int, ink: str) -> None:
    g.fill(circle(7.5, 12, 6), rule="")
    g.fill(circle(6.5, 12, 2), ink)
    g.fill("M12.5 10H22.5V14H20.5V17H17.5V14H12.5Z")


def _printer(g: Grid, n: int, ink: str) -> None:
    g.fill(rrect(6, 2, 12, 5.5, 1))
    g.fill(rrect(1.5, 8.5, 21, 9.5, 2.5))
    g.fill(rrect(6, 14, 12, 8, 1))
    g.line("M8.5 18H15.5", ink)


def _themes(g: Grid, n: int, ink: str) -> None:
    g.fill("M12 1.5C5.9 1.5 1 6.2 1 12S5.9 22.5 12 22.5C13.8 22.5 14.8 21.2 14.2 19.7"
           "C13.5 18 14.5 16.5 16.2 16.5H19C21.2 16.5 23 14.7 23 12.2C23 6.2 18.1 1.5 12 1.5Z")
    spots = ((7, 8.5), (12.5, 6), (17.5, 9.5), (6, 14.5)) if n > 16 else ((7, 9), (15, 8))
    for x, y in spots:
        g.fill(circle(x, y, 1.8), ink)


def _wallpaper(g: Grid, n: int, ink: str) -> None:
    g.fill(rrect(1.5, 3.5, 21, 14, 2.5))
    g.fill("M3.5 15C7 9.5 11 13 14 10.5C16.5 8.5 19 8 20.5 8.5V15.5H3.5Z", ink)
    g.line("M8 21.5H16")


def _accessibility(g: Grid, n: int, ink: str) -> None:
    g.fill(circle(12, 4, 2.8))
    g.line("M3 9L12 11L21 9M12 11V15.5M12 15.5L7.5 22M12 15.5L16.5 22")


def _log_viewer(g: Grid, n: int, ink: str) -> None:
    g.line("M3 5H21M3 11H12M3 17H9")
    g.fill("M17.5 9.5L23 20H12Z")
    if n > 16:
        g.line("M17.5 13.5V16.5", ink)


def _clock(g: Grid, n: int, ink: str) -> None:
    g.fill(circle(12, 12, 11))
    g.line("M12 5.5V12L16.5 14.5", ink)


def _recorder(g: Grid, n: int, ink: str) -> None:
    g.fill(rrect(8, 1.5, 8, 13.5, 4))
    g.line("M3.5 11.5A8.5 8.5 0 0 0 20.5 11.5M12 20V22.5")


def _paint(g: Grid, n: int, ink: str) -> None:
    g.fill("M20.7 1.7L22.6 3.6L12.3 15.6L8.7 12Z")
    g.fill("M7.5 13.5L10.8 16.8C10.8 20.2 8.5 22.5 3.6 22.5C2 22.5 1.4 22 1.4 22"
           "C3 21 3 19.2 3.6 17.5C4.2 15.3 5.6 13.7 7.5 13.5Z")


def _character_map(g: Grid, n: int, ink: str) -> None:
    g.line("M3 22L12 2.5L21 22M6.5 15H17.5")


def _system_info(g: Grid, n: int, ink: str) -> None:
    g.fill(circle(12, 12, 11))
    g.fill(circle(12, 6.8, 1.9), ink)
    g.line("M12 11.5V18", ink)


def _remote_desktop(g: Grid, n: int, ink: str) -> None:
    g.fill(rrect(1.5, 3, 21, 14, 2.5))
    g.line("M8 21.5H16")
    g.line("M8 12.5L15 7.5M10.5 7.5H15V12", ink)


def _scan(g: Grid, n: int, ink: str) -> None:
    g.line("M5 8.5V4A2 2 0 0 1 7 2H17A2 2 0 0 1 19 4V8.5")
    g.line("M1.5 12H22.5")
    g.line("M5 15.5V20A2 2 0 0 0 7 22H17A2 2 0 0 0 19 20V15.5")


def _phone(g: Grid, n: int, ink: str) -> None:
    g.fill(rrect(5.5, 1, 13, 22, 3))
    g.fill(rrect(7.5, 3.5, 9, 14.5, 0.8), ink)
    if n > 16:
        g.fill(circle(12, 20.4, 1.2), ink)


def _restore(g: Grid, n: int, ink: str) -> None:
    g.line("M3.5 12A9 9 0 1 0 6.3 5.5L3 8.5M3 3V8.5H8.5")
    if n > 16:
        g.line("M12 7.5V12L15.5 14")


def _run(g: Grid, n: int, ink: str) -> None:
    g.fill(rrect(1.5, 3, 21, 18, 3))
    g.fill(rrect(3.5, 7.5, 17, 11.5, 1), ink)
    g.fill(poly((9.5, 9.5), (16, 13.2), (9.5, 17)))


EVERYDAY = {
    "system-file-manager": ("violet", _files, 0.7),
    "utilities-terminal": ("graphite", _terminal, 0.64),
    "accessories-text-editor": ("blue", _text_editor, 0.66),
    "accessories-calculator": ("green", _calculator, 0.72),
    "accessories-screenshot": ("magenta", _screenshot, 0.66),
    "multimedia-photo-viewer": ("teal", _photos, 0.72),
    "document-viewer": ("rose", _document_viewer, 0.7),
    "multimedia-video-player": ("rose", _media_player, 0.62),
    "multimedia-audio-player": ("magenta", _music, 0.66),
    "utilities-file-archiver": ("amber", _archive, 0.68),
    "partitionmanager": ("indigo", _disks, 0.68),
    "utilities-system-monitor": ("teal", _task_manager, 0.7),
    "help-browser": ("indigo", _help, 0.62),
    "kwalletmanager": ("amber", _passwords, 0.72),
    "printer": ("graphite", _printer, 0.7),
    "preferences-desktop-theme": ("magenta", _themes, 0.72),
    "preferences-desktop-wallpaper": ("indigo", _wallpaper, 0.72),
    "preferences-desktop-accessibility": ("blue", _accessibility, 0.68),
    "utilities-log-viewer": ("graphite", _log_viewer, 0.68),
    "raye-clock": ("indigo", _clock, 0.7),
    "raye-sound-recorder": ("rose", _recorder, 0.68),
    "raye-paint": ("amber", _paint, 0.7),
    "accessories-character-map": ("graphite", _character_map, 0.62),
    "raye-system-info": ("blue", _system_info, 0.7),
    "raye-remote-desktop": ("blue", _remote_desktop, 0.72),
    "raye-scan": ("teal", _scan, 0.66),
    "raye-phone": ("green", _phone, 0.74),
    "raye-restore": ("amber", _restore, 0.66),
    "system-run": ("graphite", _run, 0.7),
}
TONE_OF = {name: spec[0] for name, spec in EVERYDAY.items()}


# ------------------------------------------------------------- categories

def category(colours: tuple[str, ...], draw) -> callable:
    def make(n: int) -> Svg:
        s, m, size = tile(n, colours)
        draw(Grid(s, m, size, 0.6), n)
        return s
    return make


def _dev(g: Grid, n: int) -> None:
    g.line("M8 6L2 12L8 18M16 6L22 12L16 18")
    if n >= 22:
        g.line("M14 4L10 20", opacity=0.8)


def _games(g: Grid, n: int) -> None:
    g.fill("M7 6H17A6 6 0 0 1 17 18C15 18 14.5 16 12 16C9.5 16 9 18 7 18A6 6 0 0 1 7 6Z")
    g.line("M5 12H9M7 10V14", VIOLET)
    g.fill(circle(16.5, 10.5, 1.2), VIOLET)
    g.fill(circle(18.5, 13.5, 1.2), VIOLET)


def _graphics(g: Grid, n: int) -> None:
    g.fill("M20.5 2.5L22 4L12 16L9 13Z")
    g.fill("M8 14L11 17C11 20 9 22 4.5 22C2.5 22 2 21.5 2 21.5C3.5 20.5 3.5 19 4 17.5C4.6 15.6 6 14 8 14Z")


def _internet(g: Grid, n: int) -> None:
    g.line(circle(12, 12, 10))
    g.line("M2 12H22M12 2C8.5 6 8.5 18 12 22C15.5 18 15.5 6 12 2")


def _multimedia(g: Grid, n: int) -> None:
    g.fill(rrect(1, 4, 22, 16, 3))
    g.fill(poly((9.5, 8), (16.5, 12), (9.5, 16)), VIOLET)


def _office(g: Grid, n: int) -> None:
    g.fill(rrect(2, 7, 20, 14, 2.5))
    g.line("M8 7V5A2 2 0 0 1 10 3H14A2 2 0 0 1 16 5V7")
    g.fill(rrect(2, 12, 20, 1.6, 0), VIOLET, 0.6)


def _other(g: Grid, n: int) -> None:
    for x in (6, 18):
        for y in (6, 18):
            g.fill(rrect(x - 4, y - 4, 8, 8, 2))


def _system(g: Grid, n: int) -> None:
    g.fill(gear_path(12, 12, 11, 8.5, 8 if n > 16 else 6, 4), rule="evenodd")


def _utilities(g: Grid, n: int) -> None:
    g.fill("M15.5 2A6.5 6.5 0 0 0 9.4 10.7L2.6 17.5A2.1 2.1 0 0 0 5.6 20.5L12.4 13.7"
           "A6.5 6.5 0 0 0 21.2 7.6L17.4 11.4L13.6 10.4L12.6 6.6L16.4 2.8A6.5 6.5 0 0 0 15.5 2Z")


CATEGORIES = {
    "applications-development": category(("#5B22D6", VIOLET), _dev),
    "applications-games": category((VIOLET, GLOW), _games),
    "applications-graphics": category(("#9A3CFF", GLOW), _graphics),
    "applications-internet": category(("#4B3CFF", "#7B6BFF"), _internet),
    "applications-multimedia": category((GLOW, "#E060C0"), _multimedia),
    "applications-office": category(("#6A2EEB", "#9A6BFF"), _office),
    "applications-other": category(("#5E5C66", "#7E7C88"), _other),
    "applications-system": category(("#3F3E45", "#5E5C66"), _system),
    "applications-utilities": category(("#6A2EEB", GLOW), _utilities),
}
CATEGORY_ALIASES = {
    "applications-accessories": "applications-utilities",
    "applications-education": "applications-other",
    "applications-science": "applications-other",
    "applications-engineering": "applications-development",
    "applications-network": "applications-internet",
    "applications-all": "applications-other",
    "applications-system-symbolic": "applications-system",
    "preferences-desktop": "applications-system",
    "preferences-system-network": "applications-internet",
}

# ------------------------------------------------------------- the set

APPS = {
    "preferences-system": settings,
    "system-software-install": store,
    "system-software-update": update,
    "raye-installer": installer,
    "raye-windows-apps": windows_apps,
    "raye-app-chooser": app_chooser,
    **{name: app(tone, draw, box) for name, (tone, draw, box) in EVERYDAY.items()},
}

# The names apps and launchers really ask for (their .desktop Icon=).
APP_ALIASES = {
    # Raye's own apps
    "systemsettings": "preferences-system",
    "org.raye.store": "system-software-install",
    "plasmadiscover": "system-software-install",
    "plasma-discover": "system-software-install",
    "org.raye.update": "system-software-update",
    "raye-update": "system-software-update",
    "system-installer": "raye-installer",
    "calamares": "raye-installer",
    "org.raye.installer": "raye-installer",
    # the launcher of Windows programs asks for this name; nobody sees it
    "wine": "raye-windows-apps",
    "org.raye.windows": "raye-windows-apps",
    # the "Open with" dialog of the desktop portal asks for KDE's logo
    "kde": "raye-app-chooser",
    # KDE apps by their reverse-DNS ids
    "org.kde.dolphin": "system-file-manager",
    "org.raye.files": "system-file-manager",
    "file-manager": "system-file-manager",
    "org.kde.konsole": "utilities-terminal",
    "konsole": "utilities-terminal",
    "terminal": "utilities-terminal",
    "org.kde.kate": "accessories-text-editor",
    "kate": "accessories-text-editor",
    "org.kde.kwrite": "accessories-text-editor",
    "kwrite": "accessories-text-editor",
    "text-editor": "accessories-text-editor",
    "org.kde.kcalc": "accessories-calculator",
    "kcalc": "accessories-calculator",
    "org.kde.spectacle": "accessories-screenshot",
    "spectacle": "accessories-screenshot",
    "applets-screenshooter": "accessories-screenshot",
    "org.kde.gwenview": "multimedia-photo-viewer",
    "gwenview": "multimedia-photo-viewer",
    "image-viewer": "multimedia-photo-viewer",
    "org.kde.okular": "document-viewer",
    "okular": "document-viewer",
    "org.kde.haruna": "multimedia-video-player",
    "haruna": "multimedia-video-player",
    "video-player": "multimedia-video-player",
    "org.kde.elisa": "multimedia-audio-player",
    "elisa": "multimedia-audio-player",
    "org.kde.ark": "utilities-file-archiver",
    "ark": "utilities-file-archiver",
    "org.kde.partitionmanager": "partitionmanager",
    "org.kde.plasma-systemmonitor": "utilities-system-monitor",
    "plasma-systemmonitor": "utilities-system-monitor",
    "ksysguard": "utilities-system-monitor",
    "org.kde.khelpcenter": "help-browser",
    "khelpcenter": "help-browser",
    "org.kde.kwalletmanager": "kwalletmanager",
    "org.kde.kwalletmanager5": "kwalletmanager",
    "kwalletmanager2": "kwalletmanager",
    "printer-config": "printer",
    "preferences-devices-printer": "printer",
    "org.kde.ksystemlog": "utilities-log-viewer",
    "ksystemlog": "utilities-log-viewer",
    "preferences-desktop-color": "preferences-desktop-theme",
    "org.kde.kclock": "raye-clock",
    "kclock": "raye-clock",
    "org.kde.krecorder": "raye-sound-recorder",
    "krecorder": "raye-sound-recorder",
    "org.kde.kolourpaint": "raye-paint",
    "kolourpaint": "raye-paint",
    "org.kde.kcharselect": "accessories-character-map",
    "kcharselect": "accessories-character-map",
    "hwinfo": "raye-system-info",
    "org.kde.kinfocenter": "raye-system-info",
    "kinfocenter": "raye-system-info",
    "org.kde.krdc": "raye-remote-desktop",
    "krdc": "raye-remote-desktop",
    "org.kde.skanpage": "raye-scan",
    "skanpage": "raye-scan",
    "kdeconnect": "raye-phone",
    "org.kde.kdeconnect": "raye-phone",
    "btrfs-assistant": "raye-restore",
}

ICONS = IconSet(
    icons={"apps": APPS, "categories": CATEGORIES},
    aliases={"apps": APP_ALIASES, "categories": CATEGORY_ALIASES},
)
