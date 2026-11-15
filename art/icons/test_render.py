# Every common file type gets an icon you can see, at every size, in both
# themes. A file in icons/ is not enough: Files draws what Qt's SVG engine
# makes of it, on a white view (Raye Day) and on a dark one (Raye Night).
# This draws each icon with Qt itself, on both, and fails on one that
# leaves the square empty.
#
#   python3 -m pytest art/icons/test_render.py
#
# The drawing is done by a QML test (qmltestrunner), the same Qt SVG code
# Files uses. Without qmltestrunner the test is skipped, except in the
# package build, where a skip would mean the check never ran.

import json
import os
import subprocess

import pytest

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
ICONS = os.path.join(REPO, "icons")
THEMES = ("Raye", "Raye-Dark")
# size folder -> pixels drawn
SIZES = {"16": 16, "22": 22, "24": 24, "32": 32, "48": 48, "scalable": 64}

# The types people open every day, and every icon name the MIME database
# gives them (older and newer names both, as both are still around).
TYPES = {
    "pdf": ["application-pdf"],
    "docx": ["application-vnd.openxmlformats-officedocument.wordprocessingml.document"],
    "odt": ["application-vnd.oasis.opendocument.text"],
    "xlsx": ["application-vnd.openxmlformats-officedocument.spreadsheetml.sheet"],
    "txt": ["text-plain"],
    "png": ["image-png"],
    "jpg": ["image-jpeg"],
    "zip": ["application-zip"],
    "iso": ["application-x-cd-image", "application-vnd.efi.iso",
            "application-x-iso9660-image"],
    "deb": ["application-vnd.debian.binary-package", "application-x-deb"],
    "exe": ["application-x-ms-dos-executable", "application-x-msdownload",
            "application-vnd.microsoft.portable-executable",
            "application-x-dosexec"],
}

# White for the light view, the Raye Night view colour for the dark one.
BACKGROUNDS = {"light": "#ffffff", "dark": "#1b1529"}
# A pixel counts as drawn when it is this far from the background
# (sum of the three channels, 0-765), and an icon needs this share of its
# square drawn. A page with a coloured badge covers far more; an empty or
# all-white square covers nothing on white.
MIN_DIFF = 60
MIN_SHARE = 0.05


def cases():
    out = []
    for theme in THEMES:
        for ext, names in TYPES.items():
            for name in names:
                for folder, px in SIZES.items():
                    out.append({"theme": theme, "type": ext, "name": name,
                                "size": folder, "px": px,
                                "file": os.path.join(ICONS, theme, "mimetypes",
                                                     folder, name + ".svg")})
    return out


def test_every_common_type_has_a_file():
    missing = [c["file"] for c in cases() if not os.path.exists(c["file"])]
    assert not missing, missing


QML = """
import QtQuick
import QtTest

Item {
    id: root
    width: 200; height: 200

    property var cases: %(cases)s
    property var backgrounds: %(backgrounds)s

    // The icon is a child of the square that gets grabbed, so the grab
    // holds it whatever else moves in this file.
    Rectangle {
        id: bg; width: 96; height: 96
        Image { id: img; cache: false; asynchronous: false; smooth: true }
    }

    TestCase {
        name: "IconRender"
        when: windowShown

        function test_icons() {
            var bad = []
            for (var i = 0; i < root.cases.length; ++i) {
                var c = root.cases[i]
                img.source = ""
                img.sourceSize = Qt.size(c.px, c.px)
                img.width = c.px; img.height = c.px
                img.source = "file://" + c.file
                if (img.status !== Image.Ready) {
                    bad.push(c.theme + " " + c.size + " " + c.name + ": not loaded")
                    continue
                }
                for (var b in root.backgrounds) {
                    bg.width = c.px; bg.height = c.px
                    bg.color = root.backgrounds[b]
                    waitForRendering(img)
                    var shot = grabImage(bg)
                    var drawn = 0
                    for (var y = 0; y < c.px; ++y) {
                        for (var x = 0; x < c.px; ++x) {
                            var p = shot.pixel(x, y)
                            var d = Math.abs(p.r - bg.color.r) + Math.abs(p.g - bg.color.g)
                                    + Math.abs(p.b - bg.color.b)
                            if (d * 255 > %(min_diff)d)
                                ++drawn
                        }
                    }
                    var share = drawn / (c.px * c.px)
                    if (share < %(min_share)f)
                        bad.push(c.theme + " " + c.size + " " + c.name + " on " + b
                                 + ": " + Math.round(share * 100) + "%% drawn")
                }
            }
            compare(bad.join("\\n"), "")
        }
    }
}
"""


def find_runner():
    r = subprocess.run(["sh", "-c", ". tests/find-qmltestrunner.sh && raye_find_qmltestrunner >&2 && echo \"$QMLTESTRUNNER\""],
                       cwd=REPO, capture_output=True, text=True)
    return r.stdout.strip() if r.returncode == 0 else ""


def svg_plugin():
    """Path of Qt's SVG image plugin, or "" when this Qt has none."""
    dirs = [d for d in os.environ.get("QT_PLUGIN_PATH", "").split(":") if d]
    for tool in ("qtpaths6", "qtpaths"):
        try:
            r = subprocess.run([tool, "--query", "QT_INSTALL_PLUGINS"],
                               capture_output=True, text=True)
        except OSError:
            continue
        if r.returncode == 0 and r.stdout.strip():
            dirs.append(r.stdout.strip())
            break
    for d in dirs:
        path = os.path.join(d, "imageformats", "libqsvg.so")
        if os.path.exists(path):
            return path
    return ""


def run_qml(tmp_path, items):
    runner = find_runner()
    if not runner:
        # debian/rules exports DEB_BUILD_MAINT_OPTIONS: in the package build
        # a missing runner is a failure, not a skip
        if os.environ.get("DEB_BUILD_MAINT_OPTIONS"):
            pytest.fail("qmltestrunner not found in the package build")
        pytest.skip("needs qmltestrunner")
    # Without the SVG plugin no icon loads at all, and the QML test then
    # calls every one of them "not loaded": a failure about this machine
    # that reads like one about the icons. Say what is missing instead.
    if not svg_plugin():
        if os.environ.get("DEB_BUILD_MAINT_OPTIONS"):
            pytest.fail("Qt's SVG image plugin (qt6-svg-plugins) is not "
                        "installed in the package build")
        pytest.skip("needs Qt's SVG image plugin (qt6-svg-plugins)")
    qml = tmp_path / "tst_icon_render.qml"
    qml.write_text(QML % {"cases": json.dumps(items),
                          "backgrounds": json.dumps(BACKGROUNDS),
                          "min_diff": MIN_DIFF, "min_share": MIN_SHARE})
    env = dict(os.environ, QT_QPA_PLATFORM="offscreen")
    r = subprocess.run([runner, "-input", str(qml)], capture_output=True,
                       text=True, env=env, timeout=600)
    return r.returncode, r.stdout + r.stderr


def test_every_common_type_draws_something(tmp_path):
    items = [c for c in cases() if os.path.exists(c["file"])]
    rc, out = run_qml(tmp_path, items)
    # the runner must have run the one test and passed it, not just exited 0
    assert rc == 0 and "PASS   : qmltestrunner::IconRender::test_icons()" in out, out
