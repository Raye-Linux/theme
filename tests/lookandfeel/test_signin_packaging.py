# What the sign-in screens need from the packages. Each line here is a file
# the design relies on and that nothing else would notice missing: the lock
# timing falls back to Plasma's defaults, the sign-in screen falls back to
# the stock face, the first sign-in just never happens.
#
#   python3 -m pytest -q tests/lookandfeel/test_signin_packaging.py

import os
import re
import subprocess

import pytest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def read(*parts):
    with open(os.path.join(ROOT, *parts), encoding="utf-8") as f:
        return f.read()


def install_lines(package):
    env = dict(os.environ)
    out = subprocess.run(["sh", os.path.join("debian", package + ".install")], cwd=ROOT,
                         env=env, capture_output=True, text=True, check=True)
    return out.stdout.splitlines()


def package_stanza(name):
    text = read("debian", "control")
    m = re.search(r"^Package: %s\n(.*?)(?:\n\n|\Z)" % re.escape(name), text, re.S | re.M)
    assert m, name
    return m.group(1)


@pytest.mark.parametrize("source,dest", [
    ("config/kscreenlockerrc", "etc/xdg"),
    ("config/powerdevilrc", "etc/xdg"),
    ("session/raye-first-login.sh", "etc/xdg/plasma-workspace/env"),
])
def test_raye_theme_ships(source, dest):
    assert os.path.exists(os.path.join(ROOT, source))
    assert "%s %s" % (source, dest) in install_lines("raye-theme")


def test_the_splash_finds_the_marker_through_qtcore():
    assert "import QtCore" in read("lookandfeel", "org.raye.night.desktop", "contents", "splash", "Splash.qml")
    assert "qml6-module-qtcore" in package_stanza("raye-theme")


# sddm owns this exact path. Installing over it without a diversion stops
# the whole package install with a file conflict.
FACE = "/usr/share/sddm/faces/.face.icon"


def test_the_default_face_replaces_the_stock_one():
    rules = read("debian", "rules")
    assert re.search(r"install -Dm644 assets/signin/face-512\.png\s+debian/sddm-theme-raye%s" % re.escape(FACE), rules)
    preinst = read("debian", "sddm-theme-raye.preinst")
    assert "dpkg-divert --package sddm-theme-raye --add --rename" in preinst
    assert FACE in preinst
    postrm = read("debian", "sddm-theme-raye.postrm")
    assert "dpkg-divert --package sddm-theme-raye --remove --rename" in postrm
    assert FACE in postrm


def test_the_stock_names_are_checked_in_the_build():
    rules = read("debian", "rules")
    assert re.search(r"^\tsh debian/check-stock-names$", rules, re.M)
    assert "check-stock-names" in rules[rules.index("override_dh_auto_test:"):rules.index("override_dh_auto_install:")]


# --- the check itself, against files we make ---------------------------

LOCK = """property bool viewVisible
property bool suspendToRamSupported
property bool suspendToDiskSupported
signal suspendToRam()
signal suspendToDisk()
signal clearPassword()
property string notification
"""
LOGOUT = """signal logoutRequested()
signal haltRequested()
signal haltUpdateRequested()
signal suspendRequested(int spdMethod)
signal rebootRequested()
signal rebootUpdateRequested()
signal cancelRequested()
signal lockScreenRequested()
sdtype maysd canLogout spdMethods softwareUpdatePending screenGeometry
"""
ENUM = """ShutdownTypeDefault = -1,
ShutdownTypeNone = 0,
ShutdownTypeReboot = 1,
ShutdownTypeHalt = 2,
ShutdownTypeLogout = 3,
"""


def tree(tmp_path, lock=LOCK, logout=LOGOUT, enum=ENUM):
    for rel, text in (
        ("usr/share/plasma/shells/org.kde.plasma.desktop/contents/lockscreen/LockScreen.qml", lock),
        ("usr/share/plasma/look-and-feel/org.kde.breeze.desktop/contents/logout/Logout.qml", logout),
        ("usr/include/kworkspace6/kworkspace.h", enum),
    ):
        p = tmp_path / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        if text is not None:
            p.write_text(text)
    return tmp_path


def check(root):
    env = dict(os.environ, RAYE_STOCK_ROOT=str(root))
    return subprocess.run(["sh", "debian/check-stock-names"], cwd=ROOT, env=env,
                          capture_output=True, text=True)


def test_check_passes_on_the_names_we_use(tmp_path):
    r = check(tree(tmp_path))
    assert r.returncode == 0, r.stderr


def test_check_reads_the_moved_aside_lock_screen(tmp_path):
    root = tree(tmp_path, lock=None)
    d = root / "usr/share/plasma/shells/org.kde.plasma.desktop/contents/lockscreen"
    (d / "LockScreen.qml.plasma").write_text(LOCK)
    (d / "LockScreen.qml").write_text("// ours\n")
    r = check(root)
    assert r.returncode == 0, r.stderr


@pytest.mark.parametrize("kwargs,word", [
    ({"lock": LOCK.replace("viewVisible", "viewShown")}, "viewVisible"),
    ({"logout": LOGOUT.replace("haltRequested", "haltAsked")}, "haltRequested"),
    ({"logout": LOGOUT.replace("sdtype", "shutdownType")}, "sdtype"),
    ({"enum": ENUM.replace("ShutdownTypeHalt = 2", "ShutdownTypeHalt = 4")}, "ShutdownTypeHalt = 2"),
    ({"logout": None}, "Logout.qml"),
])
def test_check_fails_and_names_what_changed(tmp_path, kwargs, word):
    r = check(tree(tmp_path, **kwargs))
    assert r.returncode != 0
    assert word in r.stderr
