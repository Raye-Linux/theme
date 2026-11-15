# SPDX-License-Identifier: GPL-3.0-or-later
"""The disk password screen on a TPM machine: the field waits for the
chip, and a question the chip already answered never shows later."""
import subprocess
from pathlib import Path

import pytest

pytest.importorskip("PIL")
from plyscript import Plymouth  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
THEME = REPO / "plymouth" / "raye"
HOOK = REPO / "initramfs" / "raye-plymouth"
FPS = 60
ASK = "Please unlock disk sda3_crypt:"


@pytest.fixture(scope="module")
def script(tmp_path_factory):
    root = tmp_path_factory.mktemp("root")
    themes = root / "usr/share/plymouth/themes"
    themes.mkdir(parents=True)
    (themes / "raye").symlink_to(THEME)
    r = subprocess.run(["sh", str(HOOK), "render", "en", ""],
                       env={"PATH": "/usr/bin:/bin", "RAYE_PLYMOUTH_ROOT": str(root)},
                       capture_output=True, text=True, check=True)
    return r.stdout


def tpm_boot(script):
    p = Plymouth(script, THEME, [(1366, 768)], files={"tpm.png": "tpm-marker.png"})
    p.frames(3)
    return p


def asking(p):
    return bool(p.globals["asking"])


def test_tpm_slow_chip_gets_the_field_after_20_s(script):
    p = tpm_boot(script)
    p.callback("DisplayPassword", ASK, 0.0)
    p.frames(19 * FPS)
    assert not asking(p)
    p.frames(2 * FPS)
    assert asking(p)


def test_disk_open_forgets_the_question(script):
    p = tpm_boot(script)
    p.callback("DisplayPassword", ASK, 0.0)
    p.frames(5)
    p.message("raye:disk-open")
    p.frames(40 * FPS)
    assert not asking(p)


@pytest.mark.parametrize("mode", ["updates", "system-upgrade", "firmware-upgrade", "boot"])
def test_update_screen_forgets_the_question(script, mode):
    p = tpm_boot(script)
    p.callback("DisplayPassword", ASK, 0.0)
    p.frames(5)
    p.message("raye:mode:" + mode)
    p.callback("SystemUpdate", "37")
    p.frames(40 * FPS)
    assert not asking(p)


def test_disk_open_takes_away_a_field_left_up(script):
    # a key showed the field, then the chip opened the disk anyway
    p = tpm_boot(script)
    p.callback("DisplayPassword", ASK, 1.0)
    p.frames(5)
    assert asking(p)
    p.message("raye:disk-open")
    p.frames(FPS)
    assert not asking(p)
    assert p.globals["ring_on"]


def test_next_disk_after_open_is_asked_again(script):
    p = tpm_boot(script)
    p.callback("DisplayPassword", ASK, 0.0)
    p.message("raye:disk-open")
    p.frames(5)
    p.callback("DisplayPassword", "Please unlock disk data:", 0.0)
    p.frames(21 * FPS)
    assert asking(p)
    assert not p.globals["wrong"]
