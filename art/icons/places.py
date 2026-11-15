# Places and devices of the Raye icon theme (theming 3.8, list 1): the
# folders and the drives Files shows in its side bar and in This PC.
# Drawn by lib.py; see there for how a drawing works.

import math

from lib import (GLOW, GRAPHITE, INK, METAL, VIOLET, IconSet, Svg, circle,
                 folder, half, poly, rrect)

# a screen showing the default wallpaper: graphite, one violet corner
SCREEN = ("#2C2B31", "#1B1A20", "#4B2FA0")


def _folder(mark: str | None = None, **kw):
    return lambda n: folder(n, mark, **kw)


# ------------------------------------------------------------- non folders

def starred(n: int) -> Svg:
    s = Svg(n)
    c, r_out = n / 2, n * 0.46
    r_in = r_out * 0.48
    pts = []
    for i in range(10):
        a = -math.pi / 2 + i * math.pi / 5
        r = r_out if i % 2 == 0 else r_in
        pts.append((c + r * math.cos(a), c + 0.04 * n + r * math.sin(a)))
    d = poly(*pts)
    if n >= 32:
        s.shadow(d, n / 40, 0.3, blur=n / 48)
    s.path(d, fill=s.linear(0, 0, n, n, ("#E7A6FF", GLOW, VIOLET)))
    if n >= 22:
        s.path(d, stroke="#FFFFFF", width=1, opacity=0.35)
    return s


def bookmarks(n: int) -> Svg:
    s = Svg(n)
    w = round(n * 0.56)
    x0, x1 = (n - w) / 2, (n + w) / 2
    top, bot = round(n * 0.06), n - round(n * 0.06)
    notch = round(n * 0.22)
    d = poly((x0, top), (x1, top), (x1, bot), (n / 2, bot - notch), (x0, bot))
    if n >= 32:
        s.shadow(d, n / 40, 0.3, blur=n / 48)
    s.path(d, fill=s.linear(x0, top, x1, bot, ("#B48CFF", VIOLET, "#5B22D6")))
    s.path(f"M{x0 + 0.5} {top}V{bot - 0.8}", stroke="#FFFFFF", width=1, opacity=0.35, cap="butt")
    return s


def _bin(n: int, full: bool) -> Svg:
    s = Svg(n)
    m = round(n * 0.19)
    lid_y = round(n * 0.2)
    lid_h = max(2, round(n * 0.1))
    top, bot = lid_y + lid_h, n - max(1, round(n * 0.06))
    taper = round(n * 0.06)
    body = poly((m, top), (n - m, top), (n - m - taper, bot), (m + taper, bot))
    if full:
        # crumpled papers above the rim
        sheets = [(n * 0.38, top - n * 0.08, n * 0.17), (n * 0.6, top - n * 0.12, n * 0.19),
                  (n * 0.48, top - n * 0.03, n * 0.15)]
        for cx, cy, r in sheets:
            d = poly((cx - r, cy + r * 0.3), (cx - r * 0.4, cy - r), (cx + r * 0.6, cy - r * 0.7),
                     (cx + r, cy + r * 0.2), (cx + r * 0.2, cy + r))
            s.path(d, fill=s.linear(cx - r, cy - r, cx + r, cy + r, ("#FFFFFF", "#DEDDE3")),
                   stroke="#A2A0AA", width=1 if n < 48 else 1.5)
    if n >= 32:
        s.shadow(body, n / 40, 0.3, blur=n / 48)
    s.path(body, fill=s.linear(m, top, n - m, bot, ("#F0EFF2", "#D3D2D6", "#B8B7BD")))
    s.path(body, stroke="#817F8B", width=1, opacity=0.8)
    if n >= 22:
        k = 3 if n < 32 else 4
        for i in range(1, k):
            x = m + taper / 2 + (n - 2 * m - taper) * i / k
            x = half(x)
            s.path(f"M{x} {top + n * 0.1}V{bot - n * 0.08}", stroke="#94929C", width=1,
                   opacity=0.6, cap="round")
    lid = rrect(m - round(n * 0.04), lid_y, n - 2 * m + 2 * round(n * 0.04), lid_h,
                max(0.5, lid_h / 3))
    s.path(lid, fill=s.linear(0, lid_y, 0, lid_y + lid_h, ("#B48CFF", VIOLET)))
    if n >= 22:
        hw = round(n * 0.12)
        s.path(rrect(n / 2 - hw, lid_y - max(1, round(n * 0.06)), 2 * hw,
                     max(1, round(n * 0.06)) + 0.5, 0.5), fill=VIOLET)
    return s


def network_globe(n: int) -> Svg:
    """network-workgroup: a violet globe."""
    s = Svg(n)
    c, r = n / 2, n * 0.42
    d = circle(c, c, r)
    if n >= 32:
        s.shadow(d, n / 40, 0.3, blur=n / 48)
    s.path(d, fill=s.radial(c - r * 0.4, c - r * 0.45, r * 1.6, ("#C9A8FF", VIOLET, "#3E1699")))
    w = 1 if n < 32 else n / 32
    col = "#FFFFFF"
    s.path(f"M{c - r} {c}H{c + r}", stroke=col, width=w, opacity=0.7)
    s.path(f"M{c} {c - r}C{c - r * 0.7} {c - r * 0.4} {c - r * 0.7} {c + r * 0.4} {c} {c + r}"
           f"M{c} {c - r}C{c + r * 0.7} {c - r * 0.4} {c + r * 0.7} {c + r * 0.4} {c} {c + r}",
           stroke=col, width=w, opacity=0.7)
    if n >= 22:
        s.path(f"M{c - r * 0.87} {c - r * 0.5}H{c + r * 0.87}M{c - r * 0.87} {c + r * 0.5}"
               f"H{c + r * 0.87}", stroke=col, width=w, opacity=0.5)
    return s


# ------------------------------------------------------------- devices

def _graphite(s: Svg, d: str, x0: float, y0: float, x1: float, y1: float) -> None:
    """A graphite body with a light rim along its top: readable on Raye
    Night as well as on Raye Day."""
    n = s.n
    if n >= 32:
        s.shadow(d, n / 40, 0.35, blur=n / 48)
    s.path(d, fill=s.linear(x0, y0, x0, y1, GRAPHITE))
    s.path(d, stroke=s.linear(0, y0, 0, y1, [(0, "#FFFFFF", 0.45), (0.4, "#FFFFFF", 0.12),
                                              (1, "#000000", 0.25)]), width=1)


def computer(n: int) -> Svg:
    s = Svg(n)
    m = max(1, round(n * 0.06))
    top, scr_b = round(n * 0.12), round(n * 0.7)
    frame = rrect(m, top, n - 2 * m, scr_b - top, max(1, n / 16))
    _graphite(s, frame, m, top, n - m, scr_b)
    b = max(1, round(n * 0.06))
    screen = rrect(m + b, top + b, n - 2 * m - 2 * b, scr_b - top - 2 * b, max(0.5, n / 32))
    s.path(screen, fill=s.linear(m, top, n - m, scr_b, SCREEN))
    if n >= 22:
        cx, cy = n / 2, (top + scr_b) / 2
        s.path(circle(cx, cy, n * 0.12), fill=s.radial(cx, cy, n * 0.2,
                                                      [(0, "#FFFFFF", 0.9), (1, "#FFFFFF", 0)]))
    stand_w = round(n * 0.18)
    s.path(poly((n / 2 - stand_w / 2, scr_b), (n / 2 + stand_w / 2, scr_b),
                (n / 2 + stand_w / 2 + 1, n - round(n * 0.16)), (n / 2 - stand_w / 2 - 1, n - round(n * 0.16))),
           fill=s.linear(0, scr_b, 0, n, ("#4F4D56", "#323137")))
    base = rrect(round(n * 0.27), n - round(n * 0.16), n - 2 * round(n * 0.27),
                 max(1, round(n * 0.07)), max(0.5, n / 40))
    _graphite(s, base, 0, n - round(n * 0.16), n, n)
    return s


def _drive_body(s: Svg) -> tuple[float, float, float, float]:
    n = s.n
    m = max(1, round(n * 0.06))
    top, bot = round(n * 0.3), n - round(n * 0.26)
    d = rrect(m, top, n - 2 * m, bot - top, max(1, n / 12))
    _graphite(s, d, m, top, n - m, bot)
    return m, top, n - m, bot


def _light(s: Svg, x: float, y: float, w: float, h: float) -> None:
    s.path(rrect(x, y, w, h, h / 2), fill=s.linear(x, y, x + w, y, (VIOLET, GLOW)))
    if s.n >= 32:
        s.path(rrect(x - 1, y - 1, w + 2, h + 2, (h + 2) / 2), fill=GLOW, opacity=0.25)


def harddisk(n: int, mark: str | None = None) -> Svg:
    s = Svg(n)
    x0, top, x1, bot = _drive_body(s)
    h = max(1, round(n * 0.06))
    _light(s, x1 - round(n * 0.12) - round(n * 0.18), bot - round(n * 0.1) - h,
           round(n * 0.18), h)
    if n >= 22:
        y = half(top + (bot - top) * 0.42)
        s.path(f"M{x0 + n * 0.1} {y}H{x1 - n * 0.1}", stroke="#000000", width=1, opacity=0.35)
    if mark == "eye":
        # the system drive carries the Raye eye
        cx, cy, r = x0 + (bot - top) * 0.55, bot - (bot - top) * 0.32, (bot - top) * 0.22
        s.path(f"M{cx - r * 1.4} {cy}Q{cx} {cy - r * 1.3} {cx + r * 1.4} {cy}"
               f"Q{cx} {cy + r * 1.3} {cx - r * 1.4} {cy}Z", fill="#FFFFFF", opacity=0.9)
        s.path(circle(cx, cy, r * 0.55), fill=s.linear(cx - r, cy, cx + r, cy, (VIOLET, GLOW)))
    return s


def solidstate(n: int) -> Svg:
    s = Svg(n)
    m = max(1, round(n * 0.06))
    top, bot = round(n * 0.34), n - round(n * 0.3)
    d = rrect(m, top, n - 2 * m, bot - top, max(1, n / 16))
    _graphite(s, d, m, top, n - m, bot)
    if n >= 22:
        cw, ch = round(n * 0.16), round((bot - top) * 0.5)
        for i in range(3):
            x = m + round(n * 0.1) + i * (cw + max(1, round(n * 0.05)))
            s.path(rrect(x, top + (bot - top - ch) / 2, cw, ch, max(0.5, n / 48)),
                   fill="#232326", stroke="#FFFFFF", width=1 if n < 48 else 1.5, opacity=0.6)
    h = max(1, round(n * 0.06))
    _light(s, n - m - round(n * 0.2), bot - h - max(1, round(n * 0.08)), round(n * 0.12), h)
    return s


def usb(n: int) -> Svg:
    s = Svg(n)
    # a stick standing on its corner: connector up right
    w = round(n * 0.36)
    plug = rrect(n * 0.5 - w * 0.34, n * 0.08, w * 0.68, n * 0.26, max(0.5, n / 48))
    body = rrect(n * 0.5 - w / 2, n * 0.3, w, n * 0.62, max(1, n / 10))
    g = f'<g transform="rotate(35 {n / 2} {n / 2})">'
    sub = Svg(n)
    sub.defs = s.defs
    sub._ids = 100
    sub.path(plug, fill=sub.linear(0, 0, n, 0, METAL), stroke="#918F9A", width=1)
    if n >= 22:
        sub.path(rrect(n * 0.5 - w * 0.2, n * 0.13, w * 0.12, n * 0.07, 0.3), fill="#716F7B")
        sub.path(rrect(n * 0.5 + w * 0.08, n * 0.13, w * 0.12, n * 0.07, 0.3), fill="#716F7B")
    sub.path(body, fill=sub.linear(0, n * 0.3, 0, n, GRAPHITE))
    sub.path(body, stroke="#FFFFFF", width=1, opacity=0.25)
    sub.path(rrect(n * 0.5 - w * 0.22, n * 0.78, w * 0.44, max(1, n * 0.05), n * 0.025),
             fill=sub.linear(0, 0, n, 0, (VIOLET, GLOW)))
    s.raw(g + "".join(sub.body) + "</g>")
    return s


def optical(n: int) -> Svg:
    s = Svg(n)
    c, r = n / 2, n * 0.44
    d = circle(c, c, r)
    if n >= 32:
        s.shadow(d, n / 40, 0.3, blur=n / 48)
    s.path(d, fill=s.radial(c * 0.8, c * 0.7, r * 1.4, ("#FFFFFF", "#E7E6EB", "#C5C4C9")))
    s.path(d, stroke="#95939E", width=1)
    if n >= 22:
        s.path(f"M{c - r * 0.75} {c - r * 0.3}A{r * 0.8} {r * 0.8} 0 0 1 {c - r * 0.3} {c - r * 0.75}",
               stroke=GLOW, width=max(1, n / 24), opacity=0.5)
        s.path(f"M{c + r * 0.75} {c + r * 0.3}A{r * 0.8} {r * 0.8} 0 0 1 {c + r * 0.3} {c + r * 0.75}",
               stroke="#36C5F0", width=max(1, n / 24), opacity=0.45)
    s.path(circle(c, c, r * 0.3), fill="#DCDBE1", stroke="#95939E", width=1)
    s.path(circle(c, c, max(1, r * 0.12)), fill=INK, opacity=0.8)
    return s


def sdcard(n: int) -> Svg:
    s = Svg(n)
    x0, x1 = round(n * 0.22), n - round(n * 0.22)
    y0, y1 = round(n * 0.06), n - round(n * 0.06)
    cut = round(n * 0.18)
    d = poly((x0 + cut, y0), (x1, y0), (x1, y1), (x0, y1), (x0, y0 + cut))
    _graphite(s, d, x0, y0, x1, y1)
    if n >= 22:
        k = 4
        cw = (x1 - x0 - cut - 2) / k
        for i in range(k):
            x = x0 + cut + 1 + i * cw
            s.path(rrect(x + cw * 0.15, y0 + max(1, n * 0.06), cw * 0.6, n * 0.16, 0.3),
                   fill="#E7C46A", opacity=0.9)
    _light(s, x0 + round(n * 0.12), y1 - round(n * 0.18), x1 - x0 - 2 * round(n * 0.12),
           max(1, round(n * 0.05)))
    return s


def phone(n: int) -> Svg:
    s = Svg(n)
    w = round(n * 0.56)
    x0 = (n - w) / 2
    y0, y1 = round(n * 0.04), n - round(n * 0.04)
    d = rrect(x0, y0, w, y1 - y0, max(1.5, n / 8))
    _graphite(s, d, x0, y0, x0 + w, y1)
    b = max(1, round(n * 0.06))
    scr = rrect(x0 + b, y0 + b * 1.6, w - 2 * b, y1 - y0 - b * 3.2, max(0.5, n / 16))
    s.path(scr, fill=s.linear(x0, y0, x0 + w, y1, SCREEN))
    if n >= 22:
        s.path(f"M{n / 2 - n * 0.08} {y1 - b * 0.8}H{n / 2 + n * 0.08}", stroke="#FFFFFF",
               width=1, opacity=0.6)
    return s


def folder_open(n: int) -> Svg:
    return folder(n, opened=True)


def folder_windows(n: int) -> Svg:
    # the Windows install seen from Raye: a folder with the eye in a window
    return folder(n, "windows")


ICONS = IconSet(
    icons={
        "places": {
            "folder": _folder(),
            "folder-open": folder_open,
            "user-home": _folder("home"),
            "user-desktop": _folder("desktop"),
            "folder-documents": _folder("document"),
            "folder-download": _folder("download"),
            "folder-music": _folder("music"),
            "folder-pictures": _folder("pictures"),
            "folder-videos": _folder("videos"),
            "folder-templates": _folder("templates"),
            "folder-publicshare": _folder("publicshare"),
            "folder-games": _folder("games"),
            "folder-cloud": _folder("cloud"),
            "folder-network": _folder("network"),
            "folder-remote": _folder("remote"),
            "folder-recent": _folder("recent"),
            "folder-windows": folder_windows,
            "folder-encrypted": _folder("encrypted"),
            "folder-locked": _folder("locked"),
            "starred": starred,
            "bookmarks": bookmarks,
            "user-trash": lambda n: _bin(n, False),
            "user-trash-full": lambda n: _bin(n, True),
            "network-workgroup": network_globe,
        },
        "devices": {
            "computer": computer,
            "drive-harddisk": harddisk,
            "drive-harddisk-solidstate": solidstate,
            "drive-harddisk-system": lambda n: harddisk(n, "eye"),
            "drive-removable-media-usb": usb,
            "media-optical": optical,
            "media-flash-sd-mmc": sdcard,
            "phone": phone,
        },
    },
    aliases={
        "places": {
            "folder-home": "user-home",
            "desktop": "user-desktop",
            "folder-desktop": "user-desktop",
            "folder-downloads": "folder-download",
            "folder-picture": "folder-pictures",
            "folder-image": "folder-pictures",
            "folder-images": "folder-pictures",
            "folder-video": "folder-videos",
            "folder-sound": "folder-music",
            "folder-public": "folder-publicshare",
            "folder-open-recent": "folder-recent",
            "folder-favorites": "starred",
            "favorites": "starred",
            "folder-bookmark": "bookmarks",
            "user-bookmarks": "bookmarks",
            "folder-root": "devices/drive-harddisk-system",
            "network-server": "folder-network",
            "folder-encrypted-closed": "folder-locked",
        },
        "devices": {
            "drive-harddisk-root": "drive-harddisk-system",
            "drive-removable-media": "drive-removable-media-usb",
            "drive-removable-media-usb-pendrive": "drive-removable-media-usb",
            "media-removable": "drive-removable-media-usb",
            "media-optical-cd": "media-optical",
            "media-optical-dvd": "media-optical",
            "media-optical-bd": "media-optical",
            "media-optical-data": "media-optical",
            "media-optical-audio": "media-optical",
            "media-flash": "media-flash-sd-mmc",
            "media-flash-memory-stick": "media-flash-sd-mmc",
            "smartphone": "phone",
            "computer-laptop": "computer",
            "drive-harddisk-ssd": "drive-harddisk-solidstate",
            "drive-harddisk-nvme": "drive-harddisk-solidstate",
        },
    },
)
