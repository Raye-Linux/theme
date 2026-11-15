# SPDX-License-Identifier: GPL-3.0-or-later
"""rayerc: the decoration hides its title bar for Files, which puts its
tabs in a title row of its own. Without the exception the tabs fall back
under a second title bar."""
import configparser
import re
from pathlib import Path

RC = Path(__file__).resolve().parents[2] / "style/kstyle/rayerc"


def exceptions():
    rc = configparser.ConfigParser(interpolation=None, strict=False)
    rc.optionxform = str
    rc.read(RC)
    return [rc[s] for s in rc.sections() if s.startswith("Windeco Exception ")]


def test_files_has_no_decoration_title_bar():
    for ex in exceptions():
        # KConfig reads "\\." as a regex "\."
        pattern = ex.get("ExceptionPattern", "").replace("\\\\", "\\")
        if ex.get("Enabled") == "true" and ex.get("ExceptionType") == "0" \
                and re.fullmatch(pattern, "org.kde.dolphin"):
            assert ex.get("HideTitleBar") == "true"
            return
    raise AssertionError("no exception hides the title bar of Files")
