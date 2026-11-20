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


def test_a_reload_after_the_install_never_says_zero(script):
    # plymouthd stays in updates mode: we change words by message, never by
    # change-mode. When the splash is shown again (the restart after the
    # install), the theme loads anew in that mode, without a number: it
    # used to say "Working on updates 0%" until the PC went down
    p = screen(script, "updates")
    p.frames(FPS)
    assert words(p) == ("", "")
    p.message("raye:mode:reboot")
    p.frames(FPS)
    assert words(p) == (script[1]["reboot"], "")


def test_the_update_start_still_shows_its_number(script):
    # the initramfs hook sends the mode and 0 the moment the disk is open
    p = screen(script, "updates")
    p.message("raye:mode:updates")
    p.callback("SystemUpdate", "0")
    p.frames(FPS)
    assert words(p)[0].endswith("%") and " 0" in words(p)[0].replace("\xa0", " ")


def test_a_reload_in_upgrade_mode_comes_back_as_an_upgrade(script):
    p = screen(script, "system-upgrade")
    p.frames(FPS)
    assert words(p) == ("", "")
    p.callback("SystemUpdate", "40")
    p.frames(FPS)
    assert script[1]["upgrade"].split("{")[0].strip() in words(p)[0]
    assert "40" in words(p)[0]


# A slow step of an install (the start-up image of each kernel rebuilt, a
# driver built, the boot menu written, the restore point) holds the same
# number for minutes: the screen says why, then goes back to the usual note.
# Reported 2026-11-20: "the first update is a black screen for a long time".

def test_a_slow_step_says_it_can_take_a_few_minutes(script):
    p = screen(script, "boot")
    p.message("raye:mode:updates")
    p.callback("SystemUpdate", "96")
    p.message("raye:slow-step")
    p.frames(FPS)
    assert words(p) == (script[1]["updates"].replace("{n}", "96"), script[1]["slow_step_note"])
    # the number keeps moving under the slow note
    p.callback("SystemUpdate", "97")
    p.frames(FPS)
    assert words(p) == (script[1]["updates"].replace("{n}", "97"), script[1]["slow_step_note"])
    p.message("raye:slow-step-done")
    p.frames(FPS)
    assert words(p) == (script[1]["updates"].replace("{n}", "97"), script[1]["updates_note"])


def test_the_slow_note_ends_with_the_install(script):
    p = screen(script, "boot")
    p.message("raye:mode:updates")
    p.message("raye:slow-step")
    p.message("raye:mode:reboot")
    p.frames(FPS)
    assert words(p) == (script[1]["reboot"], "")


def test_the_slow_note_tells_what_and_how_long():
    for lang in ("en", "fr"):
        words = dict(l.split("=", 1) for l in (THEME / f"strings-{lang}").read_text()
                     .splitlines() if "=" in l and not l.startswith("#"))
        assert words["slow_step_note"].strip()
    en = (THEME / "strings-en").read_text()
    assert "slow_step_note=Preparing the new version of the system." in en
