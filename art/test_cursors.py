# Tests of the Raye pointers: the drawings art/cursors.py makes, the
# Xcursor writer, and the files committed in cursors/.
#
#   python3 -m pytest art/test_cursors.py
#
# Only the check that redraws the Xcursor images needs rsvg-convert and
# Pillow; it is skipped without them.

import json
import os
import shutil
import struct
import sys
import xml.etree.ElementTree as ET

import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cursors  # noqa: E402

THEMES = sorted(cursors.THEMES)

# every name of theming 3.10, and the ones toolkits ask for most
WANTED = {
    "default", "left_ptr", "arrow", "top_left_arrow", "pointer", "hand1",
    "hand2", "pointing_hand", "text", "xterm", "ibeam", "vertical-text",
    "wait", "watch", "progress", "left_ptr_watch", "help", "question_arrow",
    "crosshair", "cross", "tcross", "not-allowed", "forbidden", "no-drop",
    "crossed_circle", "move", "all-scroll", "fleur", "size_all", "grab",
    "openhand", "grabbing", "closedhand", "n-resize", "s-resize", "e-resize",
    "w-resize", "ne-resize", "nw-resize", "se-resize", "sw-resize",
    "size_ver", "size_hor", "size_bdiag", "size_fdiag", "ns-resize",
    "ew-resize", "nesw-resize", "nwse-resize", "col-resize", "row-resize",
    "split_h", "split_v", "alias", "copy", "dnd-copy", "link", "dnd-link",
    "context-menu", "cell", "zoom-in", "zoom-out", "pencil", "color-picker",
    "up-arrow", "X_cursor", "pirate", "top_left_corner", "top_right_corner",
    "bottom_left_corner", "bottom_right_corner", "top_side", "bottom_side",
    "left_side", "right_side", "dnd-move", "dnd-none", "dnd-no-drop", "dnd-ask",
    "target", "ul_angle", "ur_angle", "ll_angle", "lr_angle", "sb_up_arrow",
    "sb_down_arrow", "sb_left_arrow", "sb_right_arrow", "kill", "wayland-cursor",
}

# every cursor keyword of CSS (but auto and none): what GTK apps and web
# pages ask for
CSS = {
    "default", "context-menu", "help", "pointer", "progress", "wait", "cell",
    "crosshair", "text", "vertical-text", "alias", "copy", "move", "no-drop",
    "not-allowed", "grab", "grabbing", "e-resize", "n-resize", "ne-resize",
    "nw-resize", "s-resize", "se-resize", "sw-resize", "w-resize", "ew-resize",
    "ns-resize", "nesw-resize", "nwse-resize", "col-resize", "row-resize",
    "all-scroll", "zoom-in", "zoom-out",
}

# pointers whose hotspot is the arrow tip
TIPPED = {"default", "progress", "help", "alias", "copy", "link", "dnd-move",
          "dnd-no-drop", "context-menu"}
# pointers whose hotspot is the middle of a symmetric drawing
CENTRED = {"text", "crosshair", "cell", "not-allowed", "all-scroll",
           "size_ver", "size_hor", "size_bdiag", "size_fdiag", "col-resize",
           "row-resize", "x-cursor", "wait", "vertical-text"}


def xdir(theme: str) -> str:
    return os.path.join(cursors.OUT, theme, "cursors")


def sdir(theme: str) -> str:
    return os.path.join(cursors.OUT, theme, "cursors_scalable")


def load(theme: str, name: str) -> list[dict]:
    with open(os.path.join(xdir(theme), name), "rb") as f:
        return cursors.read_xcursor(f.read())


def alpha(img: dict, x: int, y: int) -> int:
    if not (0 <= x < img["width"] and 0 <= y < img["height"]):
        return 0
    return img["pixels"][4 * (y * img["width"] + x) + 3]


# ------------------------------------------------------------- the writer

def test_xcursor_round_trip_and_premultiply():
    rgba = bytes([255, 0, 0, 255, 255, 255, 255, 128, 0, 0, 0, 0, 10, 20, 30, 40])
    data = cursors.xcursor([(24, 2, 2, 1, 0, 0, rgba), (48, 2, 2, 1, 1, 40, rgba)])
    assert data[:4] == b"Xcur"
    assert struct.unpack_from("<3I", data, 4) == (16, 0x10000, 2)
    imgs = cursors.read_xcursor(data)
    assert [(i["size"], i["xhot"], i["yhot"], i["delay"]) for i in imgs] == \
        [(24, 1, 0, 0), (48, 1, 1, 40)]
    px = imgs[0]["pixels"]
    assert px[0:4] == bytes([0, 0, 255, 255])        # red, BGRA in memory
    assert px[4:8] == bytes([128, 128, 128, 128])    # white at half: premultiplied
    assert px[8:12] == bytes([0, 0, 0, 0])


def test_reader_refuses_broken_files():
    good = cursors.xcursor([(24, 1, 1, 0, 0, 0, bytes(4))])
    for bad in (b"XCUR" + good[4:], good[:-2], good[:16]):
        with pytest.raises((ValueError, struct.error)):
            cursors.read_xcursor(bad)


def test_drawings_are_valid_svg():
    for theme, colours in cursors.THEMES.items():
        for name in cursors.drawings():
            for f in range(cursors.frames(name)):
                root = ET.fromstring(cursors.svg(name, colours, f))
                assert root.get("viewBox") == "0 0 32 32", name


def test_hotspots_inside_the_canvas():
    for name in cursors.drawings():
        x, y = cursors.hotspot(name)
        assert 0 <= x < cursors.CANVAS and 0 <= y < cursors.CANVAS, name
        for size in cursors.SIZES:
            hx, hy = cursors.hotspot_px((x, y), size)
            assert 0 <= hx < cursors.image_px(size) and 0 <= hy < cursors.image_px(size)


def test_arrow_is_nineteen_pixels_at_nominal_size():
    ys = [float(p.split()[1]) for p in cursors.ARROW[1:-2].split(" L")]
    assert 19 <= max(ys) - min(ys) <= 20


def test_aliases_point_at_drawings():
    names = set(cursors.drawings())
    seen = set()
    for target, aliases in cursors.ALIASES.items():
        assert target in names
        for a in aliases:
            assert a not in names and a not in seen, a
            seen.add(a)
    assert WANTED <= names | seen
    assert len(names) >= 39


# ------------------------------------------------------------- committed files

@pytest.mark.parametrize("theme", THEMES)
def test_every_name_is_installed(theme):
    names = set(cursors.drawings())
    every = names | {a for v in cursors.ALIASES.values() for a in v}
    assert set(os.listdir(xdir(theme))) == every
    assert set(os.listdir(sdir(theme))) == every | {cursors.RING}
    for target, aliases in cursors.ALIASES.items():
        for a in aliases:
            for d in (xdir(theme), sdir(theme)):
                p = os.path.join(d, a)
                assert os.path.islink(p) and os.readlink(p) == target, a
    for n in names:
        assert not os.path.islink(os.path.join(xdir(theme), n))


@pytest.mark.parametrize("theme", THEMES)
def test_every_css_keyword_is_installed(theme):
    missing = sorted(n for n in CSS if not os.path.exists(os.path.join(xdir(theme), n))
                     or not os.path.exists(os.path.join(sdir(theme), n, "metadata.json")))
    assert not missing, missing


@pytest.mark.parametrize("theme", THEMES)
def test_drag_pointers_are_not_the_forbidden_sign(theme):
    # no-drop is a drag over a place that refuses it: the arrow with a
    # small sign, not the big circle of a button that does nothing
    assert os.readlink(os.path.join(xdir(theme), "no-drop")) == "dnd-no-drop"
    # dnd-ask: the drop opens the Move here / Copy here menu
    assert os.readlink(os.path.join(xdir(theme), "dnd-ask")) == "context-menu"


@pytest.mark.parametrize("theme", THEMES)
def test_drag_pointers_match_windows(theme):
    # moving shows the plain arrow, copying adds a plus, linking the
    # small arrow of a shortcut
    def target(name):
        return os.readlink(os.path.join(xdir(theme), name))
    assert target("dnd-move") == "default"
    assert target("dnd-none") == "default"
    assert target("dnd-copy") == "copy"
    assert target("dnd-link") == "alias"


@pytest.mark.parametrize("theme", THEMES)
def test_bodies_have_light(theme):
    text = cursors.svg("default", cursors.THEMES[theme], 0)
    assert '<linearGradient id="b"' in text and 'fill="url(#b)"' in text


@pytest.mark.parametrize("theme", THEMES)
def test_theme_shares_the_icon_theme_index(theme):
    # pointers and icons of one name live in one folder; the icon theme
    # owns its index.theme, so this one must not ship a second
    assert not os.path.exists(os.path.join(cursors.OUT, theme, "index.theme"))
    assert os.path.isfile(os.path.join(cursors.ROOT, "icons", theme, "index.theme"))
    assert sorted(os.listdir(os.path.join(cursors.OUT, theme))) == \
        ["cursors", "cursors_scalable"]


@pytest.mark.parametrize("theme", THEMES)
def test_xcursor_files(theme):
    for name in cursors.drawings():
        imgs = load(theme, name)
        n = cursors.frames(name)
        assert sorted({i["size"] for i in imgs}) == list(cursors.SIZES), name
        for size in cursors.SIZES:
            frames = [i for i in imgs if i["size"] == size]
            assert len(frames) == n, name
            px = cursors.image_px(size)
            for i in frames:
                assert (i["width"], i["height"]) == (px, px)
                assert (i["xhot"], i["yhot"]) == cursors.hotspot_px(cursors.hotspot(name), size)
                assert i["delay"] == (cursors.FRAME_MS if n > 1 else 0)
                p = i["pixels"]
                assert all(max(p[k], p[k + 1], p[k + 2]) <= p[k + 3]
                           for k in range(0, len(p), 4)), (name, "not premultiplied")
        if n > 1:
            first = [i["pixels"] for i in imgs if i["size"] == 24]
            assert len(set(first)) == n, (name, "frames do not move")


@pytest.mark.parametrize("theme", THEMES)
def test_tip_hotspots_are_exact(theme):
    for name in TIPPED:
        for img in load(theme, name):
            hx, hy = img["xhot"], img["yhot"]
            assert alpha(img, hx, hy) >= 128, (name, img["size"])
            assert alpha(img, hx, hy - 2) < 64, (name, img["size"])
            assert alpha(img, hx - 2, hy - 2) < 64, (name, img["size"])


@pytest.mark.parametrize("theme", THEMES)
def test_centred_hotspots_are_exact(theme):
    for name in CENTRED:
        for img in load(theme, name):
            sx = sy = total = 0
            for y in range(img["height"]):
                for x in range(img["width"]):
                    if alpha(img, x, y) >= 200:
                        sx, sy, total = sx + x, sy + y, total + 1
            cx, cy = sx / total, sy / total
            tol = 1.0 + img["size"] / 48
            assert abs(cx - img["xhot"]) <= tol, (name, img["size"], cx)
            assert abs(cy - img["yhot"]) <= tol, (name, img["size"], cy)


@pytest.mark.parametrize("theme", THEMES)
def test_text_cursor_hotspot_on_the_beam(theme):
    for img in load(theme, "text"):
        assert alpha(img, img["xhot"], img["yhot"]) == 255, img["size"]


@pytest.mark.parametrize("theme", THEMES)
def test_scalable_files_are_current(theme):
    colours = cursors.THEMES[theme]
    for name in cursors.drawings():
        with open(os.path.join(sdir(theme), name, "metadata.json")) as f:
            meta = json.load(f)
        n = cursors.frames(name)
        assert len(meta) == n, name
        for k, entry in enumerate(meta):
            assert entry["nominal_size"] == cursors.NOMINAL
            assert (entry["hotspot_x"], entry["hotspot_y"]) == \
                tuple(round(v, 2) for v in cursors.hotspot(name))
            if n > 1:
                assert entry["delay"] == cursors.FRAME_MS
            with open(os.path.join(sdir(theme), name, entry["filename"])) as f:
                assert f.read() == cursors.svg(name, colours, k), \
                    (name, "run art/cursors.py again")
        assert sorted(os.listdir(os.path.join(sdir(theme), name))) == \
            sorted(["metadata.json"] + [e["filename"] for e in meta])


@pytest.mark.skipif(not shutil.which("rsvg-convert"), reason="needs rsvg-convert")
def test_xcursor_images_are_current():
    pytest.importorskip("PIL")
    for theme, colours in cursors.THEMES.items():
        for name in ("default", "text", "wait"):
            img = next(i for i in load(theme, name) if i["size"] == 24)
            raw = cursors.rasterize(cursors.svg(name, colours, 0), img["width"])
            assert cursors._premultiplied_argb(raw) == img["pixels"], \
                (theme, name, "run art/cursors.py again")


# ------------------------------------------------------------- busy pointers

# The pointers of an app that starts (arrow and a small ring) and of an
# app that is busy (the ring alone). They are named here and not taken
# from cursors.frames(): a drawing that loses its animation makes
# frames() say 1, and every check above would then agree with it.
BUSY = ["progress", "left_ptr_watch", "half-busy", "wait", "watch"]


@pytest.mark.parametrize("theme", THEMES)
@pytest.mark.parametrize("name", BUSY)
def test_busy_pointers_turn(theme, name):
    imgs = load(theme, name)
    for size in cursors.SIZES:
        frames = [i for i in imgs if i["size"] == size]
        assert len(frames) >= 8, (name, size, "one still picture, not an animation")
        assert len({i["pixels"] for i in frames}) == len(frames), (name, size)
        assert all(20 <= i["delay"] <= 100 for i in frames), (name, size)
    with open(os.path.join(sdir(theme), name, "metadata.json")) as f:
        meta = json.load(f)
    assert len(meta) >= 8, name
    texts = set()
    for e in meta:
        with open(os.path.join(sdir(theme), name, e["filename"])) as f:
            texts.add(f.read())
    assert len(texts) == len(meta), (name, "scalable frames do not move")


@pytest.mark.parametrize("theme", THEMES)
def test_busy_ring_turns_smoothly(theme):
    # 30 degrees a frame stutters; Windows' ring never looks like steps
    with open(os.path.join(sdir(theme), "wait", "metadata.json")) as f:
        meta = json.load(f)
    assert len(meta) >= 24, "more than 15 degrees a frame"
    turn = sum(e["delay"] for e in meta)
    assert 800 <= turn <= 1200, turn


@pytest.mark.parametrize("theme", THEMES)
def test_busy_pointers_are_raye(theme):
    # the turning arc goes from the Raye violet to the magenta glow
    for name in ("progress", "wait"):
        text = cursors.svg(name, cursors.THEMES[theme], 0).upper()
        assert cursors.ACCENT.upper() in text and cursors.GLOW.upper() in text, name


@pytest.mark.parametrize("theme", THEMES)
def test_launch_ring_matches_the_progress_pointer(theme):
    # the ring the desktop draws beside the pointer while an app starts
    colours = cursors.THEMES[theme]
    d = os.path.join(sdir(theme), cursors.RING)
    with open(os.path.join(d, "metadata.json")) as f:
        meta = json.load(f)
    assert len(meta) == cursors.FRAMES >= 8
    assert sorted(os.listdir(d)) == sorted(["metadata.json"] + [e["filename"] for e in meta])
    texts = set()
    for k, e in enumerate(meta):
        assert (e["hotspot_x"], e["hotspot_y"]) == cursors.hotspot("progress")
        assert e["nominal_size"] == cursors.NOMINAL and e["delay"] == cursors.FRAME_MS
        with open(os.path.join(d, e["filename"])) as f:
            text = f.read()
        assert text == cursors.ring_svg(colours, k), "run art/cursors.py again"
        # the very arc of the progress pointer, frame for frame
        cx, cy, r, t = cursors.RING_AT
        assert cursors.ring_frame(cx, cy, r, t, k, colours) in cursors.svg("progress", colours, k)
        texts.add(text)
    assert len(texts) == len(meta), "the ring does not turn"
