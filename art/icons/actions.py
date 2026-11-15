# Action icons of the Raye icon theme: what the toolbars and menus of every
# app show. Drawn by lib.py; see symbolic.py for the glyph model they share
# with the status icons: a 16 unit grid, 1 unit lines, round joins, in the
# colour scheme's text colour, the accent for the one part a button acts on.
#
# Names not drawn here fall back to the parent themes in index.theme.

from lib import IconSet, circle, f, poly, rrect
from symbolic import Glyph, with_twins


def G() -> Glyph:
    return Glyph()


PAGE = "M4.5 1.5H9.5L12.5 4.5V14.5H4.5ZM9.5 1.5V4.5H12.5"
ARROW_L = "M9.5 3.5L5 8L9.5 12.5"
ARROW_R = "M6.5 3.5L11 8L6.5 12.5"
ARROW_U = "M3.5 9.5L8 5L12.5 9.5"
ARROW_D = "M3.5 6.5L8 11L12.5 6.5"
LENS = circle(7, 7, 4.5)


# The window buttons are shapes in both files: GTK title bars and
# Firefox's ask for the plain names too, and GTK only fills.

def ring(x: float, y: float, w: float, h: float, r: float) -> str:
    """A 1 unit frame on the rounded rectangle (x, y, w, h); the hole is
    drawn the other way round, so it stays a hole with either fill rule."""
    o = rrect(x - 0.5, y - 0.5, w + 1, h + 1, r + 0.5)
    ix, iy, iw, ih, ir = x + 0.5, y + 0.5, w - 1, h - 1, max(0.0, r - 0.5)
    hole = (f"M{f(ix + ir)} {f(iy)}A{f(ir)} {f(ir)} 0 0 0 {f(ix)} {f(iy + ir)}"
            f"V{f(iy + ih - ir)}A{f(ir)} {f(ir)} 0 0 0 {f(ix + ir)} {f(iy + ih)}"
            f"H{f(ix + iw - ir)}A{f(ir)} {f(ir)} 0 0 0 {f(ix + iw)} {f(iy + ih - ir)}"
            f"V{f(iy + ir)}A{f(ir)} {f(ir)} 0 0 0 {f(ix + iw - ir)} {f(iy)}Z")
    return o + hole


def bar(x0: float, y0: float, x1: float, y1: float, w: float = 1.0) -> str:
    """A straight line of width w as a filled shape, ends squared off."""
    dx, dy = x1 - x0, y1 - y0
    n = (dx * dx + dy * dy) ** 0.5
    px, py = -dy / n * w / 2, dx / n * w / 2
    pts = [(x0 + px, y0 + py), (x1 + px, y1 + py), (x1 - px, y1 - py), (x0 - px, y0 - py)]
    area = sum(a[0] * b[1] - b[0] * a[1] for a, b in zip(pts, pts[1:] + pts[:1]))
    return poly(*(pts if area > 0 else pts[::-1]))


def gear() -> Glyph:
    return (G().line(circle(8, 8, 2))
            .line("M8 1.5V3.5M8 12.5V14.5M1.5 8H3.5M12.5 8H14.5M3.4 3.4L4.8 4.8M11.2 11.2L12.6 12.6"
                  "M3.4 12.6L4.8 11.2M11.2 4.8L12.6 3.4")
            .line(circle(8, 8, 4.5)))


ACTIONS = {
    # documents
    "document-new": lambda: G().line(PAGE).line("M8.5 8V12M6.5 10H10.5", role="accent"),
    "document-open": lambda: G().line("M1.5 12.5V3.5H6L7.5 5H12.5V7")
                                .line("M1.5 12.5L3.5 7H14.5L12.5 12.5Z"),
    "document-save": lambda: G().line("M2.5 2.5H11L13.5 5V13.5H2.5Z")
                                .line("M5 2.5V6H10.5V2.5M4.5 13.5V9H11.5V13.5"),
    "document-save-as": lambda: G().line("M2.5 2.5H11L13.5 5V8M8 13.5H2.5V2.5M5 2.5V6H10.5V2.5")
                                   .line("M9.5 13.8L10 11.8L13.3 8.5L15 10.2L11.7 13.5Z", role="accent"),
    "document-print": lambda: G().line("M4.5 5.5V1.5H11.5V5.5M4.5 11.5H1.5V5.5H14.5V11.5H11.5")
                                 .line("M4.5 9.5H11.5V14.5H4.5Z"),
    "document-properties": lambda: G().line(PAGE).line("M6.5 8H10.5M6.5 10H10.5M6.5 12H9"),
    "document-share": lambda: G().line(circle(12, 3.5, 2)).line(circle(4, 8, 2)).line(circle(12, 12.5, 2))
                                 .line("M5.8 7L10.2 4.5M5.8 9L10.2 11.5"),
    "folder-new": lambda: G().line("M1.5 13.5V3H6L7.5 4.5H14.5V13.5Z")
                             .line("M8 7V11M6 9H10", role="accent"),
    "bookmark-new": lambda: G().line("M4 2H12V14L8 11L4 14Z"),
    "tab-new": lambda: G().line("M1.5 13.5V4H6V2H10.5V4M1.5 13.5H14.5V7.5")
                          .line("M13 1.5V5.5M11 3.5H15", role="accent"),
    "link": lambda: G().line("M6.5 9.5A2.8 2.8 0 0 0 10.5 9.5L13 7A2.8 2.8 0 0 0 9 3L8 4")
                       .line("M9.5 6.5A2.8 2.8 0 0 0 5.5 6.5L3 9A2.8 2.8 0 0 0 7 13L8 12"),
    "lock": lambda: G().line(rrect(3.5, 7, 9, 7, 1.5)).line("M5.5 7V4.5A2.5 2.5 0 0 1 10.5 4.5V7"),
    "unlock": lambda: G().line(rrect(3.5, 7, 9, 7, 1.5)).line("M5.5 7V4.5A2.5 2.5 0 0 1 10.3 3.5"),
    # editing
    "edit-copy": lambda: G().line(rrect(5.5, 5.5, 8, 9, 1)).line("M3.5 10.5H2.5V1.5H10.5V3.5"),
    "edit-cut": lambda: G().line(circle(4, 12, 2)).line(circle(12, 12, 2))
                           .line("M5.3 10.5L11 2M10.7 10.5L5 2"),
    "edit-paste": lambda: G().line("M5.5 2.5H3V14H13V2.5H10.5").line(rrect(5.5, 1.5, 5, 2.5, 0.8))
                             .line("M6 8H10M6 11H10"),
    "edit-undo": lambda: G().line("M5.5 2.5L2.5 5.5L5.5 8.5")
                            .line("M2.5 5.5H9.5A3.5 3.5 0 0 1 9.5 12.5H5"),
    "edit-redo": lambda: G().line("M10.5 2.5L13.5 5.5L10.5 8.5")
                            .line("M13.5 5.5H6.5A3.5 3.5 0 0 0 6.5 12.5H11"),
    "edit-delete": lambda: G().line("M2.5 4H13.5M6 4V2H10V4M4 4L4.7 14.5H11.3L12 4")
                              .line("M6.8 7V11.5M9.2 7V11.5"),
    "edit-find": lambda: G().line(LENS).line("M10.3 10.3L14 14"),
    "system-search": lambda: G().line(circle(6.5, 6.5, 4)).line("M9.5 9.5L14.5 14.5", width=1.5),
    "edit-clear": lambda: G().line("M6 3H14.5V13H6L1.5 8Z").line("M8.5 6L12 10M12 6L8.5 10"),
    "edit-rename": lambda: G().line("M8 1.5V14.5M6 1.5H10M6 14.5H10")
                              .line("M6 4H1.5V12H6M10 4H14.5V12H10"),
    "edit-select-all": lambda: G().line(rrect(4.5, 4.5, 7, 7, 0.5), role="accent")
                                  .line("M1.5 4V1.5H4M12 1.5H14.5V4M14.5 12V14.5H12M4 14.5H1.5V12"),
    # lists and moving around
    "list-add": lambda: G().line("M8 2.5V13.5M2.5 8H13.5"),
    "list-remove": lambda: G().line("M2.5 8H13.5"),
    "go-previous": lambda: G().line(ARROW_L),
    "go-next": lambda: G().line(ARROW_R),
    "go-up": lambda: G().line(ARROW_U),
    "go-down": lambda: G().line(ARROW_D),
    "go-home": lambda: G().line("M1.5 8L8 2L14.5 8M3.5 6.5V14H12.5V6.5").line("M6.5 14V10H9.5V14"),
    "view-refresh": lambda: G().line("M13.5 8A5.5 5.5 0 1 1 11.5 3.8").line("M11.5 1V4H14.5"),
    "view-fullscreen": lambda: G().line("M2 6V2H6M10 2H14V6M14 10V14H10M6 14H2V10"),
    "view-list-details": lambda: G().line("M5 3.5H14M5 8H14M5 12.5H14")
                                    .fill(circle(2.5, 3.5, 1)).fill(circle(2.5, 8, 1)).fill(circle(2.5, 12.5, 1)),
    "view-list-icons": lambda: G().line(rrect(2, 2, 5, 5, 1)).line(rrect(9, 2, 5, 5, 1))
                                  .line(rrect(2, 9, 5, 5, 1)).line(rrect(9, 9, 5, 5, 1)),
    "view-sort": lambda: G().line("M2 3.5H14M2 8H10M2 12.5H6"),
    "zoom-in": lambda: G().line(LENS).line("M10.3 10.3L14 14").line("M7 5V9M5 7H9"),
    "zoom-out": lambda: G().line(LENS).line("M10.3 10.3L14 14").line("M5 7H9"),
    "zoom-original": lambda: G().line(LENS).line("M10.3 10.3L14 14").fill(circle(7, 7, 1.4)),
    "zoom-fit-best": lambda: G().line("M2 5.5V2H5.5M10.5 2H14V5.5M14 10.5V14H10.5M5.5 14H2V10.5")
                                .line(rrect(5, 5, 6, 6, 0.5)),
    # settings and menus
    "configure": gear,
    "settings-configure": lambda: gear().fill(circle(8, 8, 0.8), role="accent"),
    "application-menu": lambda: G().line("M2.5 4H13.5M2.5 8H13.5M2.5 12H13.5"),
    # windows and dialogs
    "window-close": lambda: G().fill(bar(3.3, 3.3, 12.7, 12.7) + bar(12.7, 3.3, 3.3, 12.7)),
    "window-minimize": lambda: G().fill(rrect(2.5, 7.5, 11, 1, 0.5)),
    "window-maximize": lambda: G().fill(ring(3, 3, 10, 10, 1)),
    "window-restore": lambda: G().fill(ring(2.5, 5, 8.5, 8.5, 1))
                                 .fill(poly((5, 5), (5, 2), (14, 2), (14, 11), (11, 11),
                                            (11, 10), (13, 10), (13, 3), (6, 3), (6, 5))),
    "dialog-ok": lambda: G().line("M2.5 8.5L6.5 12.5L13.5 3.5"),
    "dialog-cancel": lambda: G().line(circle(8, 8, 6.5)).line("M3.5 3.5L12.5 12.5"),
    "dialog-close": lambda: G().line(circle(8, 8, 6.5)).line("M5.5 5.5L10.5 10.5M10.5 5.5L5.5 10.5"),
    "help-about": lambda: G().line(circle(8, 8, 6.5)).fill(circle(8, 5, 0.9)).line("M8 7.5V11.5"),
    "help-contents": lambda: G().line(circle(8, 8, 6.5))
                                .line("M6 6.2A2 2 0 1 1 9 8C8.3 8.4 8 8.8 8 9.5").fill(circle(8, 11.7, 0.8)),
    "process-stop": lambda: G().line("M5.3 1.5H10.7L14.5 5.3V10.7L10.7 14.5H5.3L1.5 10.7V5.3Z")
                               .line("M5.5 5.5L10.5 10.5M10.5 5.5L5.5 10.5"),
    # media
    "media-playback-start": lambda: G().fill("M4 2.5L13.5 8L4 13.5Z"),
    "media-playback-pause": lambda: G().fill(rrect(3.5, 2.5, 3, 11, 0.5)).fill(rrect(9.5, 2.5, 3, 11, 0.5)),
    "media-playback-stop": lambda: G().fill(rrect(3, 3, 10, 10, 1)),
    "media-skip-forward": lambda: G().fill("M2.5 3L10 8L2.5 13Z").fill(rrect(11, 3, 2.2, 10, 0.5)),
    "media-skip-backward": lambda: G().fill("M13.5 3L6 8L13.5 13Z").fill(rrect(2.8, 3, 2.2, 10, 0.5)),
    "media-record": lambda: G().fill(circle(8, 8, 5), role="negative"),
    # drawing on a picture
    "draw-freehand": lambda: G().line("M1.5 12C3.5 8 5 13 7 10S10 5 14.5 3"),
    "draw-rectangle": lambda: G().line(rrect(2, 3.5, 12, 9, 1)),
    "draw-arrow": lambda: G().line("M2.5 13.5L13 3M7 3H13V9"),
    "draw-text": lambda: G().line("M3 3.5V2.5H13V3.5M8 2.5V13.5M6 13.5H10"),
    "draw-eraser": lambda: G().line("M5.5 13.5L1.8 9.8L9 2.5L14 7.5L7.8 13.5H14"),
    "transform-crop": lambda: G().line("M4 1V12H15M1 4H12V15"),
    "object-rotate-left": lambda: G().line("M2.5 8A5.5 5.5 0 1 0 4.5 3.8").line("M4.5 1V4H1.5")
                                     .line(rrect(6.2, 6.2, 3.6, 3.6, 0.5), role="accent"),
    "object-rotate-right": lambda: G().line("M13.5 8A5.5 5.5 0 1 1 11.5 3.8").line("M11.5 1V4H14.5")
                                      .line(rrect(6.2, 6.2, 3.6, 3.6, 0.5), role="accent"),
}

ALIASES = {
    "edit-trash": "edit-delete",
    "delete": "edit-delete",
    "edit-find-replace": "edit-find",
    "search": "system-search",
    "go-previous-view": "go-previous",
    "go-next-view": "go-next",
    "open-menu": "application-menu",
    "hamburger-menu": "application-menu",
    "tools-report-bug": "help-about",
    "dialog-apply": "dialog-ok",
    "object-select": "dialog-ok",
    "checkmark": "dialog-ok",
    "window-close-symbolic": "window-close",
    "tab-close": "window-close",
    "document-save-all": "document-save",
    "view-fullscreen-restore": "window-restore",
    "view-restore": "window-restore",
    "preferences-other": "configure",
    "zoom-fit-width": "zoom-fit-best",
    "zoom-1": "zoom-original",
    "insert-link": "link",
    "emblem-shared": "document-share",
}
_PAIRS, ALIASES = with_twins(ACTIONS, ALIASES)

ICONS = IconSet(
    icons={"actions": {name: pair[0] for name, pair in _PAIRS.items()}},
    aliases={"actions": ALIASES},
    dark={"actions": {name: pair[1] for name, pair in _PAIRS.items()}},
)
