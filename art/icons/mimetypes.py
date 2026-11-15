# File types of the Raye icon theme (theming 3.8, list 2): a sheet of
# paper with the mark of its kind, and a few that are their own picture
# (pictures, archives, Windows programs). Drawn by lib.py.

from lib import (GLOW, GLYPHS, GRAPHITE, INK, VIOLET, IconSet, Svg, badge,
                 circle, glyph, half, page, page_box, rrect, text_lines)

GLYPHS.update({
    "lines": [("line", "M2.5 3.5H9.5M2.5 6H9.5M2.5 8.5H7")],
    "grid": [("line", "M2 2H10V10H2ZM2 5H10M2 7.6H10M5 2V10")],
    "chart": [("line", "M2 10H10"), ("fill", "M2.8 6H4.6V9.2H2.8Z"),
              ("fill", "M5.1 3.4H6.9V9.2H5.1Z"), ("fill", "M7.4 5H9.2V9.2H7.4Z")],
    "code": [("line", "M4 3.5L1.6 6L4 8.5M8 3.5L10.4 6L8 8.5M6.9 2.4L5.1 9.6")],
    "prompt": [("line", "M2.4 3.4L5 6L2.4 8.6M6.2 9H9.6")],
    "gear": [("line", circle(6, 6, 2.6)),
             ("line", "M6 1.4V2.6M6 9.4V10.6M1.4 6H2.6M9.4 6H10.6M2.7 2.7L3.6 3.6M8.4 8.4L9.3 9.3"
                      "M2.7 9.3L3.6 8.4M8.4 3.6L9.3 2.7")],
    "disc": [("line", circle(6, 6, 4.4)), ("fill", circle(6, 6, 1.3))],
    "box": [("line", "M2 4L6 2L10 4V8.4L6 10.4L2 8.4ZM2 4L6 6L10 4M6 6V10.4")],
    "letter": [("line", "M2.4 10.4L6 1.8L9.6 10.4M3.9 7.2H8.1")],
})

PDF = ("#F2727A", "#D63A44")
DOC = ("#6E8BFF", "#3F5BE0")
SHEET = ("#3FCB85", "#1C985A")
SLIDES = ("#FFA35C", "#E5651E")
WEB = ("#4FB4FF", "#2A7BE0")
SCRIPT = ("#9A6BFF", VIOLET)
SHELL = ("#3F3E45", "#1B1A20")
SOUND = ("#E07BFF", GLOW)
FILM = ("#5B22D6", "#2A1052")
TOOL = GRAPHITE[:2]


def _badge_geom(n: int) -> tuple[float, float, float]:
    p = page_box(n)
    size = 8 if n == 16 else round(n * 0.42)
    cx = (p.left + p.right) / 2
    cy = p.bottom - size / 2 - (1 if n == 16 else max(1, round(n * 0.08)))
    return cx, cy, size


def marked(mark: str, colour: tuple[str, str], lines: bool = True):
    """A page with a few text lines and a coloured badge carrying mark."""
    def draw(n: int) -> Svg:
        s, p = page(n)
        if lines and n >= 22:
            text_lines(s, p, 2 if n < 48 else 3)
        cx, cy, size = _badge_geom(n)
        badge(s, cx, cy, size, colour)
        glyph(s, mark, cx, cy, size * 0.78 if n > 16 else 7)
        return s
    return draw


def plain_text(n: int) -> Svg:
    s, p = page(n)
    text_lines(s, p, 4 if n <= 16 else (6 if n < 48 else 8))
    return s


def unknown(n: int) -> Svg:
    """A type nobody knows is a blank sheet, not a question to the user."""
    return page(n)[0]


def font(n: int) -> Svg:
    s, p = page(n)
    cx, cy = p.centre
    glyph(s, "letter", cx, cy + n * 0.04, round(n * 0.5) if n > 16 else 9, colour=INK)
    return s


def image(n: int) -> Svg:
    """A picture in a white frame: violet sky, a sun, two hills."""
    s = Svg(n)
    m = max(1, round(n * 0.06))
    top, bot = round(n * 0.16), n - round(n * 0.16)
    frame = rrect(m, top, n - 2 * m, bot - top, max(1, n / 16))
    if n >= 32:
        s.shadow(frame, n / 40, 0.3, blur=n / 48)
    s.path(frame, fill="#FFFFFF")
    s.path(frame, stroke="#B8B7BD", width=1)
    b = max(1, round(n * 0.06))
    x0, y0, x1, y1 = m + b, top + b, n - m - b, bot - b
    s.path(rrect(x0, y0, x1 - x0, y1 - y0, max(0.5, n / 32)),
           fill=s.linear(0, y0, 0, y1, ("#C9A8FF", "#E7D4FF")))
    sun = (x0 + (x1 - x0) * 0.72, y0 + (y1 - y0) * 0.3)
    s.path(circle(*sun, max(1, (y1 - y0) * 0.14)), fill="#FFD36B")
    w, h = x1 - x0, y1 - y0
    s.path(f"M{x0} {y1}L{x0 + w * 0.32} {y0 + h * 0.42}L{x0 + w * 0.58} {y1}Z",
           fill=s.linear(0, y0, 0, y1, (VIOLET, "#5B22D6")))
    s.path(f"M{x0 + w * 0.36} {y1}L{x0 + w * 0.66} {y0 + h * 0.55}L{x1} {y1}Z",
           fill=s.linear(0, y0, 0, y1, (GLOW, "#8A2BD6")))
    return s


def archive(n: int) -> Svg:
    """A page with a zip down its middle."""
    s, p = page(n)
    cx = half((p.left + p.right) / 2)
    w = max(2, round(n * 0.12))
    y0, y1 = p.top, p.bottom - round(n * 0.3)
    s.path(rrect(cx - w / 2, y0, w, y1 - y0, 0), fill=s.linear(0, y0, 0, y1, TOOL))
    step = max(2, round(n / 12))
    tooth = max(1, round(step * 0.45))
    y, k = y0 + step / 2, 0
    while y < y1 - step / 2:
        x = cx - w / 2 - tooth if k % 2 == 0 else cx + w / 2
        s.path(rrect(x, y, tooth, max(1, round(step * 0.5)), 0), fill="#716F7B")
        y += step
        k += 1
    pull = rrect(cx - w * 0.9, y1 - 1, w * 1.8, round(n * 0.2), max(0.5, n / 40))
    s.path(pull, fill=s.linear(0, y1, 0, y1 + n * 0.2, ("#B48CFF", VIOLET)))
    s.path(rrect(cx - w * 0.35, y1 + n * 0.06, w * 0.7, max(1, n * 0.08), n * 0.03),
           fill=INK, opacity=0.6)
    return s


def windows_program(n: int) -> Svg:
    """A program made for Windows: a window frame with the Raye eye, never
    anybody's logo."""
    s = Svg(n)
    m = max(1, round(n * 0.08))
    top, bot = round(n * 0.12), n - round(n * 0.12)
    r = max(1, n / 12)
    win = rrect(m, top, n - 2 * m, bot - top, r)
    if n >= 32:
        s.shadow(win, n / 40, 0.3, blur=n / 48)
    s.path(win, fill=s.linear(m, top, n - m, bot, ("#FFFFFF", "#EDEDF0")))
    s.path(win, stroke="#A2A0AA", width=1)
    bar = max(2, round(n * 0.16))
    s.path(f"M{m} {top + r}A{r} {r} 0 0 1 {m + r} {top}H{n - m - r}A{r} {r} 0 0 1 {n - m} {top + r}"
           f"V{top + bar}H{m}Z", fill=s.linear(m, 0, n - m, 0, (VIOLET, GLOW)))
    cx, cy = n / 2, (top + bar + bot) / 2
    rw, rh = (n - 2 * m) * 0.3, (bot - top - bar) * 0.28
    s.path(f"M{cx - rw} {cy}Q{cx} {cy - rh * 1.6} {cx + rw} {cy}Q{cx} {cy + rh * 1.6} {cx - rw} {cy}Z",
           fill=INK)
    s.path(circle(cx, cy, rh * 0.75), fill=s.radial(cx, cy, rh, (GLOW, VIOLET)))
    if n >= 22:
        s.path(circle(cx - rh * 0.25, cy - rh * 0.25, max(0.6, rh * 0.22)), fill="#FFFFFF")
    return s


ICONS = IconSet(
    icons={
        "mimetypes": {
            "text-x-generic": plain_text,
            "application-pdf": marked("lines", PDF),
            "image-x-generic": image,
            "video-x-generic": marked("videos", FILM, lines=False),
            "audio-x-generic": marked("music", SOUND, lines=False),
            "package-x-generic": archive,
            "x-office-document": marked("lines", DOC),
            "x-office-spreadsheet": marked("grid", SHEET),
            "x-office-presentation": marked("chart", SLIDES),
            "text-html": marked("network", WEB),
            "text-x-script": marked("code", SCRIPT),
            "application-x-shellscript": marked("prompt", SHELL),
            "application-x-executable": marked("gear", TOOL, lines=False),
            "application-x-ms-dos-executable": windows_program,
            "application-x-iso9660-image": marked("disc", TOOL, lines=False),
            "application-x-deb": marked("box", SCRIPT, lines=False),
            "font-x-generic": font,
            "unknown": unknown,
        },
    },
    aliases={
        "mimetypes": {
            "text-plain": "text-x-generic",
            "text-x-readme": "text-x-generic",
            "text-x-log": "text-x-generic",
            "image-png": "image-x-generic",
            "image-jpeg": "image-x-generic",
            "image-svg+xml": "image-x-generic",
            "image-gif": "image-x-generic",
            "image-webp": "image-x-generic",
            "image-bmp": "image-x-generic",
            "image-tiff": "image-x-generic",
            "image-heif": "image-x-generic",
            "video-mp4": "video-x-generic",
            "video-x-matroska": "video-x-generic",
            "video-webm": "video-x-generic",
            "audio-mpeg": "audio-x-generic",
            "audio-x-flac": "audio-x-generic",
            "audio-flac": "audio-x-generic",
            "audio-ogg": "audio-x-generic",
            "audio-x-wav": "audio-x-generic",
            "application-zip": "package-x-generic",
            "application-x-tar": "package-x-generic",
            "application-x-7z-compressed": "package-x-generic",
            "application-x-compressed-tar": "package-x-generic",
            "application-x-xz-compressed-tar": "package-x-generic",
            "application-x-bzip-compressed-tar": "package-x-generic",
            "application-x-zstd-compressed-tar": "package-x-generic",
            "application-gzip": "package-x-generic",
            "application-x-rar": "package-x-generic",
            "application-vnd.rar": "package-x-generic",
            "application-x-archive": "package-x-generic",
            "application-vnd.oasis.opendocument.text": "x-office-document",
            "application-vnd.openxmlformats-officedocument.wordprocessingml.document":
                "x-office-document",
            "application-msword": "x-office-document",
            "application-rtf": "x-office-document",
            "application-vnd.oasis.opendocument.spreadsheet": "x-office-spreadsheet",
            "application-vnd.openxmlformats-officedocument.spreadsheetml.sheet":
                "x-office-spreadsheet",
            "application-vnd.ms-excel": "x-office-spreadsheet",
            "text-csv": "x-office-spreadsheet",
            "application-vnd.oasis.opendocument.presentation": "x-office-presentation",
            "application-vnd.openxmlformats-officedocument.presentationml.presentation":
                "x-office-presentation",
            "application-vnd.ms-powerpoint": "x-office-presentation",
            "application-xhtml+xml": "text-html",
            "text-x-python": "text-x-script",
            "text-x-python3": "text-x-script",
            "application-javascript": "text-x-script",
            "text-javascript": "text-x-script",
            "text-x-csrc": "text-x-script",
            "text-x-c++src": "text-x-script",
            "application-json": "text-x-script",
            "application-x-sharedlib": "application-x-executable",
            "application-x-pie-executable": "application-x-executable",
            "application-x-msi": "application-x-ms-dos-executable",
            "application-x-msdownload": "application-x-ms-dos-executable",
            "application-x-msdos-program": "application-x-ms-dos-executable",
            "application-x-dosexec": "application-x-ms-dos-executable",
            "application-x-ms-ne-executable": "application-x-ms-dos-executable",
            "application-vnd.microsoft.portable-executable": "application-x-ms-dos-executable",
            "application-x-ms-shortcut": "application-x-ms-dos-executable",
            "application-x-cd-image": "application-x-iso9660-image",
            "application-vnd.efi.iso": "application-x-iso9660-image",
            "application-vnd.efi.img": "application-x-iso9660-image",
            "application-x-raw-disk-image": "application-x-iso9660-image",
            "application-vnd.debian.binary-package": "application-x-deb",
            "font-ttf": "font-x-generic",
            "font-otf": "font-x-generic",
            "font-woff": "font-x-generic",
            "application-x-font-ttf": "font-x-generic",
            "application-x-font-otf": "font-x-generic",
            "application-octet-stream": "unknown",
            "inode-directory": "places/folder",
        },
    },
)
