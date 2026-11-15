# The task buttons of the Plasma style (art/plasma.py): the stock task
# manager's fallback when Raye's taskbar cannot load. The file is current,
# it is the only file the script writes, every name Plasma asks for is
# there, the backplate and the pills have the sizes of the Raye taskbar,
# and nothing is drawn outside the piece that holds it.
#
#   python3 -m pytest -q art/test_plasma.py

import json
import os
import re
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET

import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import plasma  # noqa: E402

SVG_NS = "{http://www.w3.org/2000/svg}"
TEXT = plasma.tasks()
ROOT_EL = ET.fromstring(TEXT.encode())
GROUPS = {g.get("id"): g for g in ROOT_EL.iter(SVG_NS + "g")}
PREFIXES = [s[0] for s in plasma.STATES]


def test_only_the_task_buttons_are_written(tmp_path):
    # the taskbar background is art/plasmastyle.py's
    plasma.write(str(tmp_path))
    made = sorted(os.path.relpath(os.path.join(d, f), tmp_path)
                  for d, _, files in os.walk(tmp_path) for f in files)
    assert made == ["widgets/tasks.svg"]
    assert set(plasma.render_all()) == {"widgets/tasks.svg"}


def test_file_in_the_tree_is_current():
    assert plasma.stale() == [], "run python3 art/plasma.py"


def test_check_mode(tmp_path):
    script = os.path.join(plasma.ROOT, "art", "plasma.py")
    out = subprocess.run([sys.executable, "-I", "-B", script, "--check"], capture_output=True, text=True)
    assert out.returncode == 0, out.stdout + out.stderr


def test_check_mode_sees_a_stale_file(tmp_path):
    (tmp_path / "widgets").mkdir()
    (tmp_path / "widgets" / "tasks.svg").write_text("<svg/>")
    assert plasma.stale(str(tmp_path)) == ["widgets/tasks.svg"]
    assert plasma.stale(str(tmp_path / "none")) == ["widgets/tasks.svg"]


def test_every_state_plasma_asks_for():
    # Plasma's task manager asks for "<state>-hover", "hover", "<state>",
    # and "launcher-hover" for a pinned app that is not running
    for state in ("normal", "focus", "attention", "minimized", "hover", "launcher-hover",
                  "normal-hover", "focus-hover", "attention-hover", "minimized-hover"):
        for piece in plasma.NAMES:
            assert state + "-" + piece in GROUPS, state + "-" + piece
        for side in ("top", "bottom", "left", "right"):
            assert "%s-hint-%s-margin" % (state, side) in GROUPS
    assert "hint-stretch-borders" in GROUPS


def test_ids_are_unique():
    ids = re.findall(r'\bid="([^"]+)"', TEXT)
    assert len(ids) == len(set(ids))


def sizer(group):
    r = group.find(SVG_NS + "rect")
    return [float(r.get(k)) for k in ("x", "y", "width", "height")]


def numbers(d):
    return [float(v) for v in re.findall(r"-?\d+(?:\.\d+)?", d)]


def test_pieces_fit_together():
    # a column has one width and a row one height, in every state
    for prefix in PREFIXES:
        size = {n: sizer(GROUPS[prefix + "-" + n])[2:] for n in plasma.NAMES}
        for row, names in ((plasma.SIDE, ("topleft", "top", "topright")),
                           (plasma.MIDDLE_H, ("left", "center", "right")),
                           (plasma.BOTTOM, ("bottomleft", "bottom", "bottomright"))):
            for n in names:
                assert size[n][1] == row, prefix + "-" + n
        for col, names in ((plasma.SIDE, ("topleft", "left", "bottomleft")),
                           (plasma.EDGE, ("top", "center", "bottom")),
                           (plasma.SIDE, ("topright", "right", "bottomright"))):
            for n in names:
                assert size[n][0] == col, prefix + "-" + n


def bounds(element):
    """x0, y0, x1, y1 of a rect or a path made of absolute M/H/V/A moves"""
    if element.tag == SVG_NS + "rect":
        x, y, w, h = (float(element.get(k)) for k in ("x", "y", "width", "height"))
        return x, y, x + w, y + h
    xs, ys, x, y = [], [], 0.0, 0.0
    for cmd, args in re.findall(r"([MHVAZ])([^MHVAZ]*)", element.get("d")):
        v = numbers(args)
        if cmd == "M":
            x, y = v
        elif cmd == "H":
            x = v[0]
        elif cmd == "V":
            y = v[0]
        elif cmd == "A":
            nx, ny = v[5], v[6]
            if abs(nx - x) < 1e-9:
                # the round end of a pill: a half circle between two points
                # one above the other bulges out by its radius, to the left
                # going up and to the right going down (clockwise sweep)
                xs.append(x - v[0] if ny < y else x + v[0])
            x, y = nx, ny
        xs.append(x)
        ys.append(y)
    return min(xs), min(ys), max(xs), max(ys)


def test_nothing_is_drawn_outside_its_piece():
    # Plasma measures a piece by all it holds
    for name, g in GROUPS.items():
        bx, by, bw, bh = sizer(g)
        for el in list(g)[1:]:
            x0, y0, x1, y1 = bounds(el)
            assert x0 >= bx - 1e-6 and y0 >= by - 1e-6, name
            assert x1 <= bx + bw + 1e-6 and y1 <= by + bh + 1e-6, name


def test_backplate_corners_are_4px():
    for prefix in ("focus", "hover", "attention", "focus-hover"):
        for corner in ("topleft", "topright", "bottomleft", "bottomright"):
            path = GROUPS[prefix + "-" + corner].find(SVG_NS + "path")
            arc = re.search(r"A([\d.]+),([\d.]+)", path.get("d"))
            assert arc and float(arc.group(1)) == 4 == float(arc.group(2)), prefix + "-" + corner


def test_no_backplate_when_not_hovered_or_in_use():
    for prefix in ("normal", "minimized"):
        for piece in ("topleft", "top", "topright", "left", "center", "right",
                      "bottomleft", "bottomright"):
            # only the invisible size box
            assert len(list(GROUPS[prefix + "-" + piece])) == 1, prefix + "-" + piece


def pill(prefix):
    """x0, y0, x1, y1 of the pill (the last shape of the bottom edge),
    or None when the state has none"""
    shapes = list(GROUPS[prefix + "-bottom"])[1:]
    if not shapes or shapes[-1].get("rx") is None:
        return None
    return bounds(shapes[-1])


def placed(prefix, width):
    """Where the pill lands on a button `width` px wide: Plasma keeps the
    side columns as they are and stretches the bottom edge between them."""
    box_x = sizer(GROUPS[prefix + "-bottom"])[0]
    x0, _, x1, _ = pill(prefix)
    scale = (width - 2 * plasma.SIDE) / plasma.EDGE
    return (plasma.SIDE + (x0 - box_x) * scale, plasma.SIDE + (x1 - box_x) * scale)


def test_pill_sizes_on_an_icon_button():
    # Windows' sizes on a 48 px button: 6 x 3 for open apps, 16 x 3 for
    # the one in use or asking for attention
    for prefix, width in (("normal", 6), ("minimized", 6), ("normal-hover", 6), ("minimized-hover", 6),
                          ("focus", 16), ("focus-hover", 16), ("attention", 16), ("attention-hover", 16)):
        x0, y0, x1, y1 = pill(prefix)
        a, b = placed(prefix, plasma.BUTTON)
        assert round(b - a, 3) == width and round(y1 - y0, 3) == 3, prefix
    assert pill("hover") is None and pill("launcher-hover") is None


def test_pill_is_centred_under_any_button():
    # icon only, or icon and label: the pill stays under the middle of the
    # whole button, never stuck to its sides
    for prefix in ("normal", "focus", "attention", "minimized", "focus-hover"):
        for width in (40, plasma.BUTTON, 56, 120, 200, 320):
            a, b = placed(prefix, width)
            assert abs((a + b) / 2 - width / 2) < 1e-6, (prefix, width)
            assert b - a < width / 2, (prefix, width)


def test_nothing_of_the_pill_in_the_corners():
    # what sits in a corner stays at the side of a wide button
    for prefix in PREFIXES:
        for corner in ("bottomleft", "bottomright"):
            group = ET.tostring(GROUPS[prefix + "-" + corner]).decode()
            assert plasma.GLOW not in group and "ColorScheme-Highlight" not in group, prefix + corner


def test_pill_sits_2px_above_the_bottom():
    _, y0, _, y1 = pill("focus")
    box_y, box_h = sizer(GROUPS["focus-bottom"])[1], sizer(GROUPS["focus-bottom"])[3]
    assert y1 == box_y + box_h - 2


def test_app_in_use_is_accent_with_glow():
    group = ET.tostring(GROUPS["focus-bottom"]).decode()
    assert 'class="ColorScheme-Highlight"' in group
    assert plasma.GLOW in group
    assert plasma.GLOW not in ET.tostring(GROUPS["normal-bottom"]).decode()


def test_colours_follow_the_scheme():
    assert 'id="current-color-scheme"' in TEXT
    assert "#b88cff" not in TEXT
    for cls in re.findall(r'class="([^"]+)"', TEXT):
        assert cls in plasma.SCHEME


# --- against Plasma itself -------------------------------------------------

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


def test_plasma_measures_the_pieces_as_drawn(tmp_path):
    qml = shutil.which("qml6") or "/usr/lib/qt6/bin/qml"
    ksvg = "/usr/lib/%s/qt6/qml/org/kde/ksvg" % (os.environ.get("DEB_HOST_MULTIARCH")
                                                  or "x86_64-linux-gnu")
    if not (os.path.exists(qml) and os.path.isdir(ksvg)):
        pytest.skip("qml tool or the KSvg QML module not installed")
    svg = tmp_path / "tasks.svg"
    svg.write_text(TEXT)
    names = [n for n in GROUPS if not n.endswith(("margin", "borders"))]
    probe = tmp_path / "probe.qml"
    probe.write_text(PROBE % (svg, json.dumps(names)))
    env = dict(os.environ, QT_QPA_PLATFORM="offscreen", XDG_CACHE_HOME=str(tmp_path / "cache"))
    out = subprocess.run([qml, str(probe)], capture_output=True, text=True, env=env, timeout=60)
    found = re.search(r"RESULT (\{.*\})", out.stdout + out.stderr)
    assert found, out.stdout + out.stderr
    for name, (w, h) in json.loads(found.group(1)).items():
        assert [w, h] == sizer(GROUPS[name])[2:], name
