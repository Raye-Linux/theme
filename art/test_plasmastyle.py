# The surfaces of the Plasma style (art/plasmastyle.py): the files are
# current, every element has a name Plasma looks for, and the pieces of a
# frame have the sizes Plasma will measure.
#
#   python3 -m pytest -q art/test_plasmastyle.py

import os
import re
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import plasmastyle as ps  # noqa: E402

SVG_NS = "{http://www.w3.org/2000/svg}"
PIECES = ps.PIECES
SIDES = ps.SIDES
FILES = ps.render_all()


def names(text):
    return re.findall(r'\bid="([^"]+)"', text)


def element_names(text):
    """Every id but the style block and gradients (internal)."""
    return [n for n in names(text)
            if n != "current-color-scheme" and not n.startswith("raye-gradient-")]


def frame_names(prefix=""):
    return {prefix + p for p in PIECES}


def hints(prefix="", kind="margin"):
    return {"%shint-%s-%s" % (prefix, s, kind) for s in SIDES}


SHADOW = frame_names("shadow-") | hints("shadow-") | hints("shadow-", "inset")
MASK = frame_names("mask-")

# What each file must hold. The names are Plasma's (the stock style uses
# them); a file may hold nothing else, apart from hint-stretch-borders.
EXPECTED = {}
for path in ("dialogs/background.svg", "translucent/dialogs/background.svg",
             "solid/dialogs/background.svg", "widgets/tooltip.svg",
             "translucent/widgets/tooltip.svg", "solid/widgets/tooltip.svg",
             "solid/widgets/background.svg"):
    EXPECTED[path] = frame_names() | MASK | hints() | hints(kind="inset") | SHADOW
for path in ("widgets/background.svg", "translucent/widgets/background.svg"):
    EXPECTED[path] = (frame_names() | hints() | hints(kind="inset") | SHADOW
                      | {"toolbutton-pressed-center"})
for path in ("opaque/dialogs/background.svg", "opaque/widgets/tooltip.svg"):
    EXPECTED[path] = frame_names() | MASK | hints()
EXPECTED["widgets/translucentbackground.svg"] = (frame_names() | hints() | hints(kind="inset")
                                                 | SHADOW)
for path in ("widgets/panel-background.svg", "translucent/widgets/panel-background.svg",
             "solid/widgets/panel-background.svg"):
    EXPECTED[path] = (frame_names() | MASK | hints() | hints(kind="inset") | SHADOW
                      | {"thick-center"} | hints("thick-"))
EXPECTED["opaque/widgets/panel-background.svg"] = (frame_names() | MASK | hints()
                                                   | {"thick-center"} | hints("thick-"))
EXPECTED["widgets/frame.svg"] = set().union(
    *(frame_names(p) | hints(p) for p in ("plain-", "sunken-", "raised-")))
EXPECTED["widgets/plasmoidheading.svg"] = frame_names("header-") | frame_names("footer-") | hints()


def test_every_file_is_made():
    assert set(FILES) == set(EXPECTED)


def test_the_taskbar_buttons_stay_with_the_shell():
    # widgets/tasks.svg is the shell's (art/plasma.py)
    assert not any(p.endswith("tasks.svg") for p in FILES)


@pytest.mark.parametrize("path", sorted(EXPECTED))
def test_names_are_plasmas(path):
    got = element_names(FILES[path])
    assert len(got) == len(set(got)), "a name twice"
    assert set(got) == EXPECTED[path] | {"hint-stretch-borders"}


@pytest.mark.parametrize("path", sorted(EXPECTED))
def test_file_is_valid_svg_with_scheme_colours(path):
    text = FILES[path]
    root = ET.fromstring(text.encode())
    assert root.tag == SVG_NS + "svg"
    # colours from the scheme, never a stroke (a stroke makes a piece
    # bigger than its tile)
    assert "stroke" not in text
    for cls in re.findall(r'class="([^"]+)"', text):
        assert cls in ("ColorScheme-Text", "ColorScheme-Background"), cls
    assert 'id="current-color-scheme"' in text
    # no colour of another style: only the night values of the schemes,
    # black for shadows and masks, and the hint marker
    for colour in re.findall(r'#[0-9a-fA-F]{6}\b', text):
        assert colour.lower() in set(ps.SCHEME.values()) | {"#000000", "#000", "#ff00ff"}, colour


def sizer(group):
    r = group.find(SVG_NS + "rect")
    return float(r.get("width")), float(r.get("height"))


@pytest.mark.parametrize("path", sorted(EXPECTED))
def test_frame_pieces_fit_together(path):
    root = ET.fromstring(FILES[path].encode())
    groups = {g.get("id"): g for g in root.iter(SVG_NS + "g")}
    prefixes = {n[: -len("center")] for n in groups
                if n.endswith("center") and n not in ("thick-center", "toolbutton-pressed-center")}
    for prefix in prefixes:
        if prefix.startswith("shadow-"):
            continue
        tl = sizer(groups[prefix + "topleft"])
        for corner in ("topright", "bottomleft", "bottomright"):
            assert sizer(groups[prefix + corner]) == tl, prefix + corner
        assert sizer(groups[prefix + "top"])[1] == tl[1]
        assert sizer(groups[prefix + "bottom"])[1] == tl[1]
        assert sizer(groups[prefix + "left"])[0] == tl[0]
        assert sizer(groups[prefix + "right"])[0] == tl[0]


def hint_size(text, name):
    m = re.search(r'<rect id="%s" x="[^"]+" y="[^"]+" width="([^"]+)" height="([^"]+)"'
                  % re.escape(name), text)
    return float(m.group(1)), float(m.group(2))


def test_popups_are_8px_and_tooltips_4px():
    tokens = ps.load_tokens()
    root = ET.fromstring(FILES["translucent/dialogs/background.svg"].encode())
    corner = {g.get("id"): g for g in root.iter(SVG_NS + "g")}["topleft"]
    assert sizer(corner) == (tokens["radius"]["overlay"],) * 2
    root = ET.fromstring(FILES["translucent/widgets/tooltip.svg"].encode())
    corner = {g.get("id"): g for g in root.iter(SVG_NS + "g")}["topleft"]
    assert sizer(corner) == (tokens["radius"]["tooltip"],) * 2


def test_taskbar_is_square_acrylic_with_a_top_line():
    text = FILES["translucent/widgets/panel-background.svg"]
    tokens = ps.load_tokens()
    alpha = tokens["color"]["acrylic.taskbar"]["night"]["alpha"]
    assert 'fill-opacity="%s"' % ps.num(alpha) in text
    # square: no arc anywhere
    assert " A" not in text
    # the content keeps 1 px from the top line
    assert hint_size(text, "hint-top-margin")[1] == 1


def test_shadows_are_outside_the_frame_and_soft():
    text = FILES["translucent/dialogs/background.svg"]
    tokens = ps.load_tokens()
    blur, dy, _ = tokens["elevation"]["flyout"]["night"]
    assert hint_size(text, "shadow-hint-bottom-margin")[1] == blur + dy
    assert hint_size(text, "shadow-hint-top-margin")[1] == blur - dy
    assert hint_size(text, "shadow-hint-left-margin")[0] == blur
    # peak opacity between the Night and Day strengths, never more
    peaks = [float(a) for a in re.findall(r'stop-opacity="([^"]+)"', text)]
    night = tokens["elevation"]["flyout"]["night"][2]
    assert max(peaks) <= night


def test_footer_is_darker_in_both_modes():
    text = FILES["widgets/plasmoidheading.svg"]
    root = ET.fromstring(text.encode())
    center = {g.get("id"): g for g in root.iter(SVG_NS + "g")}["footer-center"]
    fills = [(r.get("fill"), r.get("class"), r.get("fill-opacity"))
             for r in center.iter(SVG_NS + "rect")][1:]
    assert fills[0] == ("#000000", None, "0.1")
    assert fills[1][1] == "ColorScheme-Background"


def test_files_in_the_tree_are_current():
    assert ps.stale() == [], "run python3 art/plasmastyle.py"


def test_check_mode():
    out = subprocess.run([sys.executable, "-I", "-B", os.path.join(ps.ROOT, "art", "plasmastyle.py"),
                          "--check"], capture_output=True, text=True)
    assert out.returncode == 0, out.stdout + out.stderr


# --- against Plasma itself -------------------------------------------------

# Read out of RAYE_ROOT when the launcher has an image open, and only
# then out of "/". These 18 checks skipped on every run of the build machine because
# Debian's plasma-workspace is not installed there -- and installing it
# would mean 203 packages and system services (udisks2 among them) on the
# machine that drives the lab, for a reference that is the WRONG Plasma
# anyway: Debian ships 6.3, Raye is built on Ubuntu's 6.6.
#
# The right reference was already to hand. tools/raye-tests mounts the
# image for every run and exports RAYE_ROOT, and the image carries the
# Plasma these files actually sit beside. The chroot the desktop is built
# in has it too (120 .svgz).
STOCK = os.path.join(os.environ.get("RAYE_ROOT", "/"),
                     "usr/share/plasma/desktoptheme/default")


@pytest.mark.parametrize("path", sorted(EXPECTED))
def test_names_exist_in_the_stock_style(path):
    """When Plasma's own style is there, every name we use is one it has
    too (or the stretch hint, which Plasma reads for every frame)."""
    stock = os.path.join(STOCK, path + "z")
    if not os.path.exists(stock):
        pytest.skip("Plasma's default style is not in %s" % STOCK)
    import gzip
    with gzip.open(stock) as f:
        theirs = set(names(f.read().decode()))
    ours = set(element_names(FILES[path])) - {"hint-stretch-borders"}
    assert ours - theirs == set(), path


PROBE = """
import QtQuick
import org.kde.ksvg as KSvg
Item {
    KSvg.Svg { id: s; imagePath: "%s" }
    Component.onCompleted: {
        const out = {}
        for (const n of %s) {
            const z = s.elementSize(n)
            out[n] = [z.width, z.height]
        }
        console.log("RESULT " + JSON.stringify(out))
        Qt.quit()
    }
}
"""


@pytest.mark.parametrize("path", ["translucent/dialogs/background.svg",
                                  "translucent/widgets/tooltip.svg",
                                  "translucent/widgets/panel-background.svg",
                                  "widgets/plasmoidheading.svg"])
def test_plasma_measures_the_pieces_as_drawn(path, tmp_path):
    """Plasma sizes each piece by its bounds as its SVG renderer sees
    them: a corner one pixel too big shifts every edge."""
    import json
    qml = shutil.which("qml6") or "/usr/lib/qt6/bin/qml"
    ksvg = "/usr/lib/%s/qt6/qml/org/kde/ksvg" % (os.environ.get("DEB_HOST_MULTIARCH")
                                                  or "x86_64-linux-gnu")
    if not (os.path.exists(qml) and os.path.isdir(ksvg)):
        pytest.skip("qml tool or the KSvg QML module not installed")
    svg = tmp_path / "frame.svg"
    svg.write_text(FILES[path])
    root = ET.fromstring(FILES[path].encode())
    groups = [g.get("id") for g in root.iter(SVG_NS + "g")]
    probe = tmp_path / "probe.qml"
    probe.write_text(PROBE % (svg, json.dumps(groups)))
    env = dict(os.environ, QT_QPA_PLATFORM="offscreen", XDG_CACHE_HOME=str(tmp_path / "cache"))
    out = subprocess.run([qml, str(probe)], capture_output=True, text=True, env=env, timeout=60)
    found = re.search(r"RESULT (\{.*\})", out.stdout + out.stderr)
    assert found, out.stdout + out.stderr
    measured = json.loads(found.group(1))
    by_id = {g.get("id"): g for g in root.iter(SVG_NS + "g")}
    for name, (w, h) in measured.items():
        assert (w, h) == sizer(by_id[name]), name
