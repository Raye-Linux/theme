# SPDX-License-Identifier: GPL-3.0-or-later
"""Session defaults that keep idle memory down.

Without a SceneGraphBackend in kdeglobals, the Plasma platform theme opens
an OpenGL context in every Qt program at start, only to check that one
can be made. Background helpers that never show a Qt Quick window
(ksmserver, kded6, ksecretd, kaccess, the portal, the admin prompt) then
kept the whole graphics driver and its threads loaded for nothing: 20 to
30 MB each, measured in a session. "opengl" is the backend Qt Quick picks
on its own; naming it only skips the check.
"""
import configparser
from pathlib import Path

TOP = Path(__file__).resolve().parents[2]


def ini(path):
    c = configparser.ConfigParser(interpolation=None, strict=False)
    c.optionxform = str
    c.read(path)
    return c


def test_qt_quick_backend_is_named():
    c = ini(TOP / "config/kdeglobals")
    assert c["QtQuickRendererSettings"]["SceneGraphBackend"] == "opengl"


def test_no_render_loop_forced():
    # QSG_RENDER_LOOP is Plasma's business on Wayland; only the backend is set
    c = ini(TOP / "config/kdeglobals")
    assert "RenderLoop" not in c["QtQuickRendererSettings"]


def test_kded_donation_nag_off():
    c = ini(TOP / "config/kded5rc")
    assert c["Module-donationmessage"]["autoload"] == "false"


def test_kded_config_is_installed():
    install = (TOP / "debian/raye-theme.install").read_text().split("\n")
    assert "config/kded5rc etc/xdg" in install
