# SPDX-License-Identifier: GPL-3.0-or-later
"""What the pointer shows while an app starts.

KWin's own startup feedback draws the app's icon, standing still, next
to the pointer. Raye draws the busy ring of its progress pointer there
instead (kwin/launchfeedback), like the arrow and small ring of Windows.
The two must not both show, and the ring must actually ship.
"""
import configparser
import json
from pathlib import Path

TOP = Path(__file__).resolve().parents[2]
EFFECT = TOP / "kwin/launchfeedback"


def ini(path):
    c = configparser.ConfigParser(interpolation=None)
    c.optionxform = str
    c.read(path)
    return c


def test_kwin_icon_feedback_is_off():
    c = ini(TOP / "config/klaunchrc")
    # BusyCursor is the switch KWin's startupfeedback reads
    assert c["FeedbackStyle"]["BusyCursor"] == "false"
    assert c["FeedbackStyle"]["TaskbarButton"] == "true"
    assert 1 <= int(c["BusyCursorSettings"]["Timeout"]) <= 60


def test_ring_effect_is_on_by_default():
    meta = json.loads((EFFECT / "metadata.json").read_text())
    # KWin names the effect after the plugin file, and warns about an Id
    assert "Id" not in meta["KPlugin"]
    assert "add_library(raye_launchfeedback MODULE" in (EFFECT / "CMakeLists.txt").read_text()
    assert meta["KPlugin"]["EnabledByDefault"] is True
    kwinrc = ini(TOP / "config/kwinrc")
    if kwinrc.has_section("Plugins"):
        assert kwinrc["Plugins"].get("raye_launchfeedbackEnabled", "true") == "true"


def test_ring_effect_is_built_and_shipped():
    rules = (TOP / "debian/rules").read_text()
    trees = next(l for l in rules.splitlines() if l.startswith("RAYE_TREES :="))
    assert "kwin/launchfeedback" in trees.split()
    lines = (TOP / "debian/raye-shell.install").read_text().splitlines()
    # not optional: a missing plugin would leave no feedback at all
    assert "usr/lib/*/qt6/plugins/kwin/effects/plugins/raye_launchfeedback.so" in lines
    assert "kwin-dev" in (TOP / "debian/control").read_text()
    cm = (EFFECT / "CMakeLists.txt").read_text()
    assert "kwin/effects/plugins" in cm and "add_test(" in cm


def test_ring_frames_ship_with_both_cursor_themes():
    for theme in ("Raye", "Raye-Dark"):
        d = TOP / "cursors" / theme / "cursors_scalable/raye-launch-ring"
        meta = json.loads((d / "metadata.json").read_text())
        assert len(meta) >= 8, theme
        assert all((d / e["filename"]).is_file() for e in meta)
