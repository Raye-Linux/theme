# SPDX-License-Identifier: GPL-3.0-or-later
"""Motion and the volume sound: what the shipped config files promise.

Windows 11 moves things quickly and never bounces; KWin's bouncy and
stretchy effects stay off, and window motion stays short. The volume keys'
own sound (plasma-pa) is off because it played again at every repeat of a
held key; the shell plays its own (rayeshellrc [Sound] VolumeChime).
"""
import configparser
from pathlib import Path

TOP = Path(__file__).resolve().parents[2]


def read(name):
    cfg = configparser.ConfigParser(strict=False, interpolation=None)
    cfg.optionxform = str
    cfg.read(TOP / "config" / name)
    return cfg


def installed(path, dest):
    lines = (TOP / "debian/raye-theme.install").read_text().splitlines()
    return any(l.lstrip("? ").split() == [path, dest] for l in lines)


def test_volume_keys_do_not_repeat_their_sound():
    assert read("plasmaparc")["General"]["AudioFeedback"] == "false"
    assert installed("config/plasmaparc", "etc/xdg")


def test_shell_plays_the_volume_sound_by_default():
    cfg = configparser.ConfigParser(strict=False, interpolation=None)
    cfg.optionxform = str
    cfg.read(TOP / "shell/lib/rayeshellrc")
    assert cfg["Sound"]["VolumeChime"] == "true"


def test_no_bouncy_window_effects():
    plugins = read("kwinrc")["Plugins"]
    for effect in ("wobblywindows", "magiclamp", "glide", "fall", "sheet"):
        assert plugins.get(effect + "Enabled", "false") == "false", effect


def test_windows_open_and_close_quickly():
    fx = read("kwinrc")["Effect-raye_windows"]
    assert int(fx["OpenDuration"]) <= 250
    assert int(fx["CloseDuration"]) < int(fx["OpenDuration"])
    assert 0.9 <= float(fx["InScale"]) < 1


def test_tray_tooltips_wait_like_the_shell():
    assert read("plasmarc")["PlasmaToolTips"]["Delay"] == "500"
