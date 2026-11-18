# Tests of the Raye status icons (art/icons/symbolic.py): every name the
# tray applets, the quick settings and the OSDs ask for is there, at
# every size, in both themes, and the drawings follow the colour scheme.
#
#   python3 -m pytest art/icons/test_symbolic.py

import os
import re
import sys
import xml.etree.ElementTree as ET

import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lib  # noqa: E402
import symbolic  # noqa: E402

THEMES = ("Raye", "Raye-Dark")
SVG = "{http://www.w3.org/2000/svg}"

# Names Plasma 6.6's network, volume, battery, Bluetooth and
# notification applets and the update notifier build or name (checked
# with `strings` on the applets and against Breeze's status folder).
REQUESTED = {
    *(f"network-wireless-connected-{v}" for v in ("00", "20", "25", "40", "50", "60", "75", "80", "100")),
    *(f"network-wireless-{v}-locked" for v in ("00", "20", "40", "60", "80", "100")),
    "network-wireless-disconnected", "network-wireless-acquiring", "network-wireless-hotspot",
    "network-wireless-off", "network-wireless-on",
    "network-wired", "network-wired-activated", "network-wired-disconnected",
    "network-vpn", "network-vpn-acquiring", "network-limited", "network-offline",
    "network-flightmode-on", "network-flightmode-off", "network-mobile-on", "network-mobile-off",
    *(f"network-mobile-{v}" for v in range(0, 101, 20)),
    "network-disconnect", "network-disconnect-symbolic",
    "audio-volume-muted", "audio-volume-low", "audio-volume-medium", "audio-volume-high",
    "audio-on", "audio-ready", "audio-input-microphone", "audio-input-microphone-symbolic",
    "microphone-sensitivity-muted", "microphone-sensitivity-low",
    "microphone-sensitivity-medium", "microphone-sensitivity-high",
    *(f"battery-{v:03d}" for v in range(0, 101, 10)),
    *(f"battery-{v:03d}-charging" for v in range(0, 101, 10)),
    "battery-missing", "battery-full", "battery-low", "battery-caution", "battery-empty",
    "battery-profile-powersave", "battery-profile-powersave-symbolic",
    "network-bluetooth", "network-bluetooth-activated", "network-bluetooth-activated-symbolic",
    "network-bluetooth-inactive-symbolic", "network-bluetooth-symbolic",
    "notification-active", "notification-active-symbolic", "notification-inactive",
    "notification-inactive-symbolic", "notifications", "notifications-disabled",
    "notifications-disabled-symbolic",
    "update-none", "update-low", "update-medium", "update-high",
    "software-update-available", "software-update-urgent",
    "brightness-high", "video-display-brightness", "redshift-status-on",
    "redshift-status-off", "redshift-status-day",
}


def status_dir(theme: str, size: str) -> str:
    return os.path.join(lib.OUT, theme, "status", size)


@pytest.mark.parametrize("theme", THEMES)
def test_requested_names_at_every_size(theme):
    for sd in lib.size_dirs():
        have = {fn[:-4] for fn in os.listdir(status_dir(theme, sd))}
        missing = REQUESTED - have
        assert not missing, (theme, sd, sorted(missing))


def test_every_name_has_a_symbolic_twin():
    names = set(symbolic._PAIRS) | set(symbolic.ALIASES)
    for name in names:
        if not name.endswith("-symbolic"):
            assert name + "-symbolic" in names, name


def test_aliases_point_at_drawings():
    for alias, target in symbolic.ALIASES.items():
        assert target in symbolic._PAIRS, (alias, target)
        assert alias not in symbolic._PAIRS, alias


def test_no_name_drawn_by_another_set():
    others = set()
    for st in lib.load_sets():
        if st is symbolic.ICONS:
            continue
        for icons in list(st.icons.values()) + list(st.aliases.values()):
            others |= set(icons)
    clash = (set(symbolic._PAIRS) | set(symbolic.ALIASES)) & others
    assert not clash, sorted(clash)


def _drawn(theme: str):
    for sd in lib.size_dirs():
        d = status_dir(theme, sd)
        for fn in sorted(os.listdir(d)):
            p = os.path.join(d, fn)
            if not os.path.islink(p):
                yield sd, p


@pytest.mark.parametrize("theme,mode", [("Raye", "light"), ("Raye-Dark", "dark")])
def test_drawings_follow_the_colour_scheme(theme, mode):
    count = 0
    for sd, p in _drawn(theme):
        count += 1
        text = open(p).read()
        root = ET.fromstring(text)
        n = lib.px_of(sd)
        assert root.get("viewBox") == f"0 0 {n} {n}", p
        st = root.find(f".//{SVG}style")
        assert st is not None and st.get("id") == "current-color-scheme", p
        assert f".ColorScheme-Text{{color:{symbolic.INK[mode]}}}" in st.text, p
        # every painted shape takes its colour from a scheme class
        for el in root.iter(f"{SVG}path"):
            if el.get("fill") in ("#FFFFFF", "#000000"):      # the badge cut-out mask
                continue
            assert el.get("class", "").startswith("ColorScheme-"), (p, el.attrib)
            colours = {el.get("fill"), el.get("stroke")} - {None, "none"}
            assert colours <= {"currentColor"}, (p, el.attrib)
        assert "href" not in text and "<text" not in text and "<image" not in text, p
    assert count == len(symbolic._PAIRS) * len(lib.size_dirs())


def test_light_and_dark_inks_differ():
    assert symbolic.INK["light"].lower() != symbolic.INK["dark"].lower()
    light = open(os.path.join(status_dir("Raye", "16"), "audio-volume-high.svg")).read()
    dark = open(os.path.join(status_dir("Raye-Dark", "16"), "audio-volume-high.svg")).read()
    assert re.sub(r"<style.*?</style>", "", light) == re.sub(r"<style.*?</style>", "", dark)
    assert light != dark


def test_meters_light_the_right_parts():
    # Wi-Fi: the arcs that are off are dim, the dot always lit
    def lit(g):
        return [op for kind, _, op, _, _ in g.parts if kind == "line"]
    assert lit(symbolic.wifi(0)) == [symbolic.DIM] * 3
    assert lit(symbolic.wifi(2)) == [1.0, 1.0, symbolic.DIM]
    assert lit(symbolic.wifi(3)) == [1.0, 1.0, 1.0]
    # battery: the fill grows with the level, low is drawn as a warning
    def fill_width(level, **kw):
        g = symbolic.battery(level, **kw)
        fills = [d for kind, d, _, role, _ in g.parts if kind == "fill"]
        return fills, [role for kind, _, _, role, _ in g.parts if kind == "fill"]
    f50, _ = fill_width(50)
    f100, _ = fill_width(100)
    assert len(f50) == 2 and len(f100) == 2          # terminal + fill
    _, roles10 = fill_width(10)
    assert "negative" in roles10
    _, roles10c = fill_width(10, charging=True)
    assert "negative" not in roles10c
    f0, _ = fill_width(0)
    assert len(f0) == 1                              # terminal only
    assert symbolic.battery(30, charging=True).cut is not None
    # mobile signal bars
    bars = [op for kind, _, op, _, _ in symbolic.mobile(2).parts if kind == "fill"]
    assert bars == [1.0, 1.0, symbolic.DIM, symbolic.DIM]


def test_wired_is_a_cable_plug_not_a_screen():
    # The screen with a stand read as "connected to a display" in quick
    # settings. The plug: a body with a latch on top, three contacts, the
    # cable leaving the middle of its foot.
    paths = [d for kind, d, *_ in symbolic.wired().parts if kind == "line"]
    assert not any("A" in d for d in paths), "a rounded box: that is the old screen"
    assert any(d.count("V") == 3 and d.count("M") == 3 for d in paths), "no contacts"
    assert any(d.endswith("M8 11.5V14") for d in paths), "no cable at the foot"
    for theme in THEMES:
        with open(os.path.join(status_dir(theme, "22"), "network-wired-activated.svg")) as fh:
            assert "M4 4H6.5V2.5H9.5V4H12V9.5H4Z" in fh.read(), theme


# GTK fills every rect and path of a symbolic icon with its own colour
# and ignores strokes: lines came out blank or as solid blobs in GTK apps.

def _symbolic_files():
    for theme in THEMES:
        for ctx in ("status", "actions"):
            for sd in lib.size_dirs():
                d = os.path.join(lib.OUT, theme, ctx, sd)
                for fn in sorted(os.listdir(d)):
                    if fn.endswith("-symbolic.svg"):
                        yield os.path.join(d, fn)


def test_symbolic_names_are_filled_shapes():
    count = 0
    for p in _symbolic_files():
        count += 1
        text = open(p, encoding="utf-8").read()
        assert "stroke" not in text, p
        root = ET.fromstring(text)
        for el in root.iter(f"{SVG}path"):
            assert el.get("fill") == "currentColor", (p, el.attrib)
        # GTK recolours rect, circle and path: a mask made of them is lost
        for m in root.iter(f"{SVG}mask"):
            assert not [el for el in m if el.tag in (f"{SVG}rect", f"{SVG}circle", f"{SVG}path")], p
    assert count > 1000


def test_outline_parses_every_command():
    got = symbolic.stretches("m1 1 l2 0 h1 v1 H1 V1.5 z M5 5 c1 0 2 1 2 2 s1 2 2 2 q1 0 1 1 t1 1"
                             " A1 1 0 0 1 13 13 a2 2 0 0 0 2 0")
    assert [len(sub) for sub in got][0] == 6
    assert got[0][-1][2] == (1.0, 1.0)            # z goes back to the start
    assert got[1][-1][2] == (15.0, 13.0)          # relative arc from (13, 13)
    assert all(st[0] == "arc" for st in got[1][-2:])
    with pytest.raises(ValueError):
        symbolic.outline("M1 1 X2 2")


def _alpha(svg_text: str, n: int):
    from PIL import Image
    import io
    import subprocess
    png = subprocess.run(["rsvg-convert", "-w", str(n), "-h", str(n)], input=svg_text.encode(),
                         capture_output=True, check=True).stdout
    return Image.open(io.BytesIO(png)).convert("RGBA").getchannel("A")


needs_rsvg = pytest.mark.skipif(not __import__("shutil").which("rsvg-convert"),
                                reason="needs rsvg-convert")


@needs_rsvg
def test_outline_covers_what_the_line_covers():
    pytest.importorskip("PIL")
    from PIL import ImageChops
    import actions
    worst = []
    for table in (symbolic.STATUS, actions.ACTIONS):
        for name, build in table.items():
            for n in (16, 24, 48):
                a = _alpha(build().svg(n, "light").text(), n)
                b = _alpha(build().svg(n, "light", gtk=True).text(), n)
                diff = ImageChops.difference(a, b)
                # Pillow 12 renamed getdata (deprecated) to get_flattened_data;
                # the build machine still has Pillow 11
                flat = getattr(diff, "get_flattened_data", diff.getdata)
                d = list(flat())
                worst.append((max(d), sum(d) / 255 / (n * n), name, n))
    # edge antialiasing only: no missing or extra part anywhere
    assert max(w[0] for w in worst) <= 80, max(worst)
    assert max(w[1] for w in worst) < 0.01, max(worst, key=lambda w: w[1])


@needs_rsvg
def test_outline_of_a_box_keeps_its_hole():
    pytest.importorskip("PIL")
    box = symbolic.Glyph().line(lib.rrect(2.5, 2.5, 11, 11, 2))
    img = _alpha(box.svg(32, "light", gtk=True).text(), 32)
    assert img.getpixel((16, 16)) == 0          # the middle stays empty
    assert img.getpixel((5, 16)) == 255         # the frame is there
