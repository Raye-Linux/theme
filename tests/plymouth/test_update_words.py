# SPDX-License-Identifier: GPL-3.0-or-later
"""The words of an update: they say so on the way down to the restart, and
once the install is over a late number from apt never brings them back."""
import subprocess
from pathlib import Path

import pytest

pytest.importorskip("PIL")
from plyscript import Plymouth  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
THEME = REPO / "plymouth" / "raye"
HOOK = REPO / "initramfs" / "raye-plymouth"
FPS = 60


@pytest.fixture(scope="module", params=["en", "fr"])
def script(request, tmp_path_factory):
    root = tmp_path_factory.mktemp("root")
    themes = root / "usr/share/plymouth/themes"
    themes.mkdir(parents=True)
    (themes / "raye").symlink_to(THEME)
    r = subprocess.run(["sh", str(HOOK), "render", request.param, ""],
                       env={"PATH": "/usr/bin:/bin", "RAYE_PLYMOUTH_ROOT": str(root)},
                       capture_output=True, text=True, check=True)
    words = dict(l.split("=", 1) for l in (THEME / f"strings-{request.param}").read_text()
                 .splitlines() if "=" in l and not l.startswith("#"))
    return r.stdout, words


def screen(script, mode):
    p = Plymouth(script[0], THEME, [(1366, 768)], mode=mode)
    p.frames(3)
    return p


def words(p):
    return p.globals["status_line"], p.globals["status_note"]


def test_restart_into_updates_says_so(script):
    p = screen(script, "reboot")
    p.message("raye:mode:prepare")
    p.frames(FPS)
    assert words(p) == (script[1]["prepare_updates"], script[1]["updates_note"])


def test_late_number_after_the_install_stays_away(script):
    p = screen(script, "boot")
    p.message("raye:mode:updates")
    p.callback("SystemUpdate", "96")
    p.frames(FPS)
    assert words(p)[0] != ""
    p.callback("SystemUpdate", "100")
    p.message("raye:mode:boot")
    p.frames(5)
    p.callback("SystemUpdate", "96")
    p.frames(FPS)
    assert words(p) == ("", "")
    assert p.globals["mode"] == "boot"


def test_late_number_on_the_way_to_a_restart_stays_away(script):
    p = screen(script, "boot")
    p.message("raye:mode:updates")
    p.callback("SystemUpdate", "100")
    p.message("raye:mode:reboot")
    p.callback("SystemUpdate", "96")
    p.frames(FPS)
    assert words(p) == (script[1]["reboot"], "")


def test_a_number_alone_still_shows_the_updates(script):
    # plymouthd started in boot mode; the first number puts the words up
    p = screen(script, "boot")
    p.callback("SystemUpdate", "37")
    p.frames(FPS)
    assert "37" in words(p)[0]
