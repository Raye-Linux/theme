# SPDX-License-Identifier: GPL-3.0-or-later
"""What a new session and a new window start from, for every account.

Windows does not reopen the apps of the last session unless asked, and a
window it opens is fully on screen. KDE defaults to the opposite on the
first (ksmserver: restorePreviousLogout), and on X11 it lets a window go
back to the position it asks for (KWin: Rules::checkIgnoreGeometry is false
unless a rule says otherwise). These lines are what change that, and each
one is silently worthless if the file is not installed.
"""
import configparser
from pathlib import Path

TOP = Path(__file__).resolve().parents[2]
INSTALL = (TOP / "debian/raye-theme.install").read_text()

# Rules::SetRule in kwin/src/rules.h
APPLY_INITIALLY = 3
FORCE = 2
# NET::NormalMask
NORMAL_WINDOWS = 1


def read(name):
    # strict=False: kwinrc repeats no key, but KConfig allows it and a
    # parse error here should be about the value, not the reader
    cfg = configparser.ConfigParser(strict=False, interpolation=None)
    cfg.optionxform = str
    cfg.read(TOP / "config" / name)
    return cfg


def installed_for_everyone(name):
    # with no "? " in front: debian/install-built skips a marked line whose
    # file is missing, and the setting would be gone without a word
    return f"\nconfig/{name} etc/xdg\n" in INSTALL


def test_a_session_starts_empty():
    assert read("ksmserverrc")["General"]["loginMode"] == "emptySession"


def test_the_session_default_is_installed():
    assert installed_for_everyone("ksmserverrc")


def test_new_windows_are_centred():
    assert read("kwinrc")["Windows"]["Placement"] == "Centered"


def test_the_placement_rule_is_listed():
    # KWin reads only the groups named in [General] rules
    rules = read("kwinrulesrc")["General"]["rules"].split(",")
    assert rules == ["raye-new-windows"]
    assert read("kwinrulesrc")["General"]["count"] == "1"


def test_a_new_window_cannot_pick_its_own_place():
    rule = read("kwinrulesrc")["raye-new-windows"]
    assert rule["ignoregeometry"] == "true"
    assert int(rule["ignoregeometryrule"]) == APPLY_INITIALLY, (
        f"not {FORCE}: Force also applies after the window is shown, in "
        "X11Window::configureRequest, and a Windows program that moves its "
        "own window could no longer do it")


def test_the_rule_leaves_dialogs_and_tool_windows_alone():
    # apps place those next to their main window, and KWin honours it
    assert int(read("kwinrulesrc")["raye-new-windows"]["types"]) == NORMAL_WINDOWS


def test_the_rule_does_not_maximize():
    # apps open at their own size: maximized on their own was tried and
    # turned back off (it sent fixed-size windows and menus to the corner)
    rule = read("kwinrulesrc")["raye-new-windows"]
    assert "maximizehoriz" not in rule and "maximizevert" not in rule


def test_the_rule_is_installed():
    assert installed_for_everyone("kwinrulesrc")
