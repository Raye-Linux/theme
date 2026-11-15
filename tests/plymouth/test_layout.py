# SPDX-License-Identifier: GPL-3.0-or-later
"""Eye, ring and words sit as one block in the middle of every screen,
in every state of the boot screen, in both languages, on screens of
different sizes, and after a screen comes or changes size.

The script runs in tests/plymouth/plyscript.py, which lays the screens
out the way plymouth does. run.sh checks the same on the real plymouthd
under Xvfb (check_block.py)."""
import subprocess
from pathlib import Path

import pytest

pytest.importorskip("PIL")
from plyscript import Plymouth  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
THEME = REPO / "plymouth" / "raye"
HOOK = REPO / "initramfs" / "raye-plymouth"
TOLERANCE = 2
FPS = 60
SETTLE = FPS  # longer than any glide

SCREENS = {
    "1080p": [(1920, 1080)],
    "laptop": [(1366, 768)],
    "laptop+monitor": [(1366, 768), (1920, 1080)],
    "fake-multi-head": [(800, 600), (640, 480)],
    "4k+1280": [(3840, 2160), (1280, 800)],
    "portrait+landscape": [(1080, 1920), (1920, 1080)],
}


@pytest.fixture(scope="module", params=["en", "fr"])
def script(request, tmp_path_factory):
    root = tmp_path_factory.mktemp("root")
    themes = root / "usr/share/plymouth/themes"
    themes.mkdir(parents=True)
    (themes / "raye").symlink_to(THEME)
    r = subprocess.run(["sh", str(HOOK), "render", request.param, ""],
                       env={"PATH": "/usr/bin:/bin", "RAYE_PLYMOUTH_ROOT": str(root)},
                       capture_output=True, text=True, check=True)
    return r.stdout


def boot(script, screens, mode="boot", tpm=False):
    files = {"tpm.png": "tpm-marker.png"} if tpm else {}
    p = Plymouth(script, THEME, screens, mode=mode, files=files)
    p.frames(3)
    return p


def password(p):
    p.callback("DisplayPassword", "Please unlock disk sda3_crypt:", 0.0)
    p.frames(2)
    p.callback("DisplayPassword", "Please unlock disk sda3_crypt:", 5.0)


def wrong_password(p):
    password(p)
    p.callback("DisplayNormal")
    p.frames(20)
    p.callback("DisplayPassword", "Please unlock disk sda3_crypt:", 0.0)


def tpm_changed(p):
    p.callback("DisplayPassword", "Please unlock disk sda3_crypt:", 0.0)
    p.frames(5)
    p.message("raye:tpm-failed")


def updates(p, n="37"):
    p.message("raye:mode:updates")
    p.callback("SystemUpdate", n)


def updates_done(p):
    updates(p, "90")
    p.frames(SETTLE)
    p.message("raye:mode:boot")


STATES = {
    "boot": ("boot", None),
    "shutdown": ("shutdown", None),
    "reboot": ("reboot", None),
    "prepare": ("reboot", lambda p: p.message("raye:mode:prepare")),
    "remove-usb": ("shutdown", lambda p: p.message("raye:remove-usb")),
    "fsck": ("boot", lambda p: p.message("raye:fsck:EFI")),
    "updates": ("boot", updates),
    "updates-100": ("boot", lambda p: updates(p, "100")),
    "finishing": ("boot", lambda p: p.message("raye:finishing-updates")),
    "updates-done": ("boot", updates_done),
    "firmware": ("boot", lambda p: p.message("raye:mode:firmware-upgrade")),
    "firmware-step": ("boot", lambda p: (p.message("raye:mode:firmware-upgrade"),
                                         p.message("raye:firmware-step:2:3"))),
    "system-upgrade": ("boot", lambda p: (p.message("raye:mode:system-upgrade"),
                                          p.callback("SystemUpdate", "37"))),
    "password": ("boot", password),
    "wrong-password": ("boot", wrong_password),
    "question": ("boot", lambda p: p.callback("DisplayQuestion", "Name:", "abc")),
    "handoff": ("boot", lambda p: p.message("raye:handoff")),
}


def off_centre(p):
    """(screen size, dx, dy) of the block on every screen."""
    out = []
    for (sx, sy, sw, sh), box in p.ink_boxes():
        assert box is not None, f"nothing drawn on the {sw}x{sh} screen"
        x0, y0, x1, y1 = box
        assert x0 > 0 and y0 > 0 and x1 < sw and y1 < sh, f"block cut on {sw}x{sh}: {box}"
        out.append(((sw, sh), (x0 + x1) / 2 - sw / 2, (y0 + y1) / 2 - sh / 2))
    return out


def assert_centred(p, what):
    for size, dx, dy in off_centre(p):
        assert abs(dx) <= TOLERANCE and abs(dy) <= TOLERANCE, \
            f"{what}: block {dx:+.1f},{dy:+.1f} px off the middle of the {size[0]}x{size[1]} screen"


@pytest.mark.parametrize("state", STATES)
@pytest.mark.parametrize("screens", SCREENS)
def test_block_in_the_middle(script, screens, state):
    mode, act = STATES[state]
    p = boot(script, SCREENS[screens], mode)
    if act:
        act(p)
    p.frames(SETTLE)
    assert_centred(p, f"{state} on {screens}")


@pytest.mark.parametrize("screens", ["laptop", "laptop+monitor"])
def test_tpm_changed_in_the_middle(script, screens):
    p = boot(script, SCREENS[screens], tpm=True)
    tpm_changed(p)
    p.frames(SETTLE)
    assert_centred(p, "tpm")


def test_quit_leaves_the_block_in_the_middle(script):
    """plymouth stops drawing at a kept quit: no glide to finish."""
    p = boot(script, SCREENS["laptop+monitor"])
    updates_done(p)
    p.frames(SETTLE)
    p.callback("Quit")
    assert_centred(p, "quit")


@pytest.mark.parametrize("state", ["boot", "updates", "password"])
def test_monitor_plugged_in_later(script, state):
    mode, act = STATES[state]
    p = boot(script, [(1366, 768)], mode)
    if act:
        act(p)
    p.frames(10)
    p.set_screens([(1366, 768), (1920, 1080)])
    p.frames(SETTLE)
    assert_centred(p, f"{state}, monitor plugged in")
    # and the driver takes over at another size
    p.set_screens([(1920, 1080), (2560, 1440)])
    p.frames(SETTLE)
    assert_centred(p, f"{state}, new sizes")


def test_same_place_on_both_screens(script):
    """The block is drawn once for all screens: the same size, the same
    distance to each edge's middle."""
    p = boot(script, SCREENS["laptop+monitor"])
    updates(p, "90")
    p.frames(SETTLE)
    (a, adx, ady), (b, bdx, bdy) = off_centre(p)
    assert abs(adx - bdx) <= 1 and abs(ady - bdy) <= 1


def test_block_glides_when_words_come(script):
    """New words move the block over a few frames, never in one jump."""
    p = boot(script, SCREENS["laptop"])
    p.frames(SETTLE)
    before = p.globals["block_top"]
    updates(p)
    p.frames(1)
    step = p.globals["block_top"]
    p.frames(SETTLE)
    after = p.globals["block_top"]
    assert after < before
    assert after < step < before
