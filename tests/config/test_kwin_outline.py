# SPDX-License-Identifier: GPL-3.0-or-later
"""config/kwinrc [Outline]: KWin's quick tile outline is Raye's.

KWin looks the path up under the data dirs and, when nothing is there,
says so only at debug level and shows no outline at all: a window dragged
to an edge would snap with nothing saying where it goes. So the path has
to name a file the shell installs.
"""
import configparser
from pathlib import Path

TOP = Path(__file__).resolve().parents[2]
# what shell/kwin/org.raye.snap/CMakeLists.txt installs under share/
INSTALLED = {"kwin-wayland/scripts/org.raye.snap": TOP / "shell/kwin/org.raye.snap/package"}


def outline_path():
    rc = configparser.ConfigParser(interpolation=None, strict=False)
    rc.optionxform = str
    rc.read(TOP / "config/kwinrc")
    return rc.get("Outline", "QmlPath", fallback=None)


def test_kwin_draws_the_raye_outline():
    path = outline_path()
    assert path, "no [Outline] QmlPath: KWin draws the stock dark outline"
    assert not path.startswith("/"), "KWin looks the path up under the data dirs"
    for prefix, source in INSTALLED.items():
        if path.startswith(prefix + "/"):
            assert (source / path[len(prefix) + 1:]).is_file(), path
            return
    raise AssertionError("%s is not a file the shell installs" % path)
