# SPDX-License-Identifier: GPL-3.0-or-later
"""raye-shortcut-defaults: Raye's shortcuts reach the account's file,
without undoing a shortcut the person picked."""
import os
import shutil
import subprocess
from pathlib import Path

import pytest

TOP = Path(__file__).resolve().parents[2]
SCRIPT = TOP / "config/raye-shortcut-defaults"
DEFAULTS = TOP / "config/kglobalshortcutsrc"

pytestmark = pytest.mark.skipif(not (shutil.which("kreadconfig6") and shutil.which("kwriteconfig6")),
                                reason="needs kreadconfig6 and kwriteconfig6")


def run(tmp_path, before=None, defaults=DEFAULTS):
    user = tmp_path / "config/kglobalshortcutsrc"
    if before is not None:
        user.parent.mkdir(parents=True, exist_ok=True)
        user.write_text(before)
    env = dict(os.environ, HOME=str(tmp_path), XDG_CONFIG_HOME=str(tmp_path / "config"),
               XDG_STATE_HOME=str(tmp_path / "state"))
    subprocess.run(["sh", str(SCRIPT), str(defaults), str(user)], env=env, check=True, timeout=60)
    return user


def read(path, *groups, key):
    args = ["kreadconfig6", "--file", str(path)]
    for g in groups:
        args += ["--group", g]
    return subprocess.run(args + ["--key", key, "--default", "MISSING"],
                          capture_output=True, text=True, check=True).stdout.rstrip("\n")


def test_first_sign_in_gets_every_default(tmp_path):
    user = run(tmp_path)
    assert read(user, "services", "org.raye.run.desktop", key="_launch") == "Meta+R"
    # recording keeps only Win+Shift+R, so Spectacle never takes Win+R
    assert read(user, "services", "org.kde.spectacle.desktop", key="RecordRegion") == "Meta+Shift+R"
    assert read(user, "services", "org.raye.inputpanel.desktop", key="Emoji") == "Meta+.\tMeta+;"
    assert read(user, "services", "org.kde.plasma.emojier.desktop", key="_launch") == "Meta+Ctrl+Alt+Shift+Space"
    assert read(user, "kwin", key="Overview") == "Meta+W\tMeta+Tab,Meta+W,Toggle Overview"


def test_the_empty_entry_of_the_clash_is_fixed(tmp_path):
    user = run(tmp_path, "[services][org.raye.run.desktop]\n_launch=\n")
    assert read(user, "services", "org.raye.run.desktop", key="_launch") == "Meta+R"


def test_shortcuts_the_person_picked_stay(tmp_path):
    user = run(tmp_path, "[kwin]\nOverview=Meta+O,Meta+W,Toggle Overview\n\n"
                         "[services][org.raye.run.desktop]\n_launch=Meta+F12\n\n"
                         "[services][org.kde.spectacle.desktop]\nRecordRegion=Meta+Alt+F9\n")
    assert read(user, "kwin", key="Overview") == "Meta+O,Meta+W,Toggle Overview"
    assert read(user, "services", "org.raye.run.desktop", key="_launch") == "Meta+F12"
    assert read(user, "services", "org.kde.spectacle.desktop", key="RecordRegion") == "Meta+Alt+F9"


def test_kwin_entry_still_at_its_stock_value_is_replaced(tmp_path):
    user = run(tmp_path, "[kwin]\nOverview=Meta+W,Meta+W,Toggle Overview\nShow Desktop=Meta+D,Meta+D,Peek at Desktop\n")
    assert read(user, "kwin", key="Overview") == "Meta+W\tMeta+Tab,Meta+W,Toggle Overview"
    assert read(user, "kwin", key="Show Desktop") == "Meta+D,Meta+D,Peek at Desktop"


def test_runs_once_per_version_of_the_defaults(tmp_path):
    user = run(tmp_path)
    user.write_text("[services][org.raye.run.desktop]\n_launch=\n")
    run(tmp_path)
    assert read(user, "services", "org.raye.run.desktop", key="_launch") == ""
    newer = tmp_path / "defaults"
    newer.write_text(DEFAULTS.read_text() + "\n[services][org.kde.kcalc.desktop]\n_launch=Launch (1)\n")
    run(tmp_path, defaults=newer)
    assert read(user, "services", "org.raye.run.desktop", key="_launch") == "Meta+R"
    assert read(user, "services", "org.kde.kcalc.desktop", key="_launch") == "Launch (1)"


def test_win_p_is_the_raye_project_panel(tmp_path):
    user = run(tmp_path)
    keys = read(user, "services", "org.raye.project.desktop", key="_launch").split("\t")
    assert "Meta+P" in keys, keys


# the stock display switcher asks for the same key in its own file: left
# alone, the daemon would hand Win+P to whichever it read first. Its action
# is "ShowOSD", not "_launch": the name the daemon gives it in the session.
def test_the_stock_display_switcher_gives_win_p_back(tmp_path):
    user = run(tmp_path)
    assert read(user, "services", "org.kde.kscreen.desktop", key="ShowOSD") == "none"


# What a fresh account's file says the first time Plasma has run: its
# activity switcher holds Win+A and Win+Q, and the Raye actions that ask
# for them are left empty by the clash. Measured in the installed system.
PLASMA_HOLDS_WIN_A = ("[plasmashell]\n"
                      "next activity=Meta+A,none,Walk through activities\n"
                      "manage activities=Meta+Q,Meta+Q,Show Activity Switcher\n"
                      "raye-quick-settings=,Meta+A,Open quick settings\n"
                      "raye-search-q=,Meta+Q,Open Search (second shortcut)\n")


def test_win_a_is_taken_back_for_quick_settings(tmp_path):
    user = run(tmp_path, PLASMA_HOLDS_WIN_A)
    assert read(user, "plasmashell", key="next activity").split(",")[0] == "none"
    assert read(user, "plasmashell", key="raye-quick-settings").split(",")[0] == "Meta+A"


def test_win_q_is_taken_back_for_search(tmp_path):
    user = run(tmp_path, PLASMA_HOLDS_WIN_A)
    assert read(user, "plasmashell", key="manage activities").split(",")[0] == "none"
    assert read(user, "plasmashell", key="raye-search-q").split(",")[0] == "Meta+Q"


# a key the person gave the activity switcher themselves stays theirs
def test_an_activity_key_the_person_picked_stays(tmp_path):
    user = run(tmp_path, "[plasmashell]\nnext activity=Meta+Alt+A,none,Walk through activities\n"
                         "raye-quick-settings=Meta+F1,Meta+A,Open quick settings\n")
    assert read(user, "plasmashell", key="next activity").split(",")[0] == "Meta+Alt+A"
    assert read(user, "plasmashell", key="raye-quick-settings").split(",")[0] == "Meta+F1"


# "none" is what Settings writes when the person clears a shortcut
def test_a_shortcut_the_person_cleared_stays_cleared(tmp_path):
    user = run(tmp_path, "[plasmashell]\nraye-quick-settings=none,Meta+A,Open quick settings\n")
    assert read(user, "plasmashell", key="raye-quick-settings").split(",")[0] == "none"


# A two-key entry left empty by a clash is written back as a lone tab:
# measured in the installed system after Win+P was held by the stock
# switcher. It is as empty as "", and fixed the same way.
def test_a_two_key_entry_emptied_by_a_clash_is_fixed(tmp_path):
    user = run(tmp_path, "[services][org.raye.project.desktop]\n_launch=\\t\n")
    assert read(user, "services", "org.raye.project.desktop", key="_launch").split("\t")[0] == "Meta+P"
