"""Everything in the shell that starts a task manager starts Raye's one
first, and falls back on the KDE monitor.

    python3 -m pytest -q tests/shell/test_task_manager_launchers.py

raye-taskmanager did not exist until 2026-11-06 -- apps/taskmanager was
an empty folder whose package built empty in silence -- so every menu of
this repo was written against org.kde.plasma-systemmonitor and three of
them still launched it by name: the taskbar menu, the clock menu, and
desktopmenu.cpp. Only shell/run/src/runparser.cpp had the chain right
("taskmgr" -> org.raye.taskmanager, then the KDE monitor).

🚨 WHY A TEST AND NOT JUST THREE EDITS. Nothing connects these call
sites: they are two plasmoids and one C++ menu, each with its own file,
and the next menu somebody writes will be written the same way the last
one was -- against the name that was right for a year. This file scans
for the KDE name instead of listing the three, so a fourth is caught the
day it appears.

⚠️ AND THE FALLBACK IS NOT OPTIONAL. raye-taskmanager is a package of
its own; it may not be installed. Launcher.launch() returns false for a
missing app, and nothing in this repo listens to Launcher.failed
(measured: the signal has no consumer), so trying Raye's first and
falling back costs nothing and shows nothing. A button that does
nothing is worse than a button that is not there.

🚨 No pytest.mark.parametrize: pytest reports an empty parameter set as
a SKIP, so a table that empties itself becomes a silent pass. The files
are walked inside one test and every failure is collected and named.
"""
import os
import pytest

RAYE = "org.raye.taskmanager"
KDE = "org.kde.plasma-systemmonitor"

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
# where a menu, a button or a shortcut can start something
SCANNED = ["shell", "config"]
SKIP_DIRS = {".git", "__pycache__", "build", "out"}
SKIP_SUFFIX = (".png", ".svg", ".jpg", ".oga", ".pf2", ".po", ".deb")

# path -> (why it may name the KDE monitor without Raye's, a string that
# must STILL be in the file for the exemption to hold). An exemption
# nobody re-checks is the defect this file is about, one level up.
EXEMPT = {
    # a test fixture: a row of made-up taskbar buttons
    "shell/plasmoids/org.raye.taskbar/tests/harness.qml": "caption",
    # fixtures of the applications-menu generator: a real KDE .desktop
    # and the category it has to land in. They test the sorting of an
    # app that exists, not which task manager Raye starts.
    "shell/menus/tests/fixtures/applications/org.kde.plasma-systemmonitor.desktop": "Exec=",
    "shell/menus/tests/expected-en.txt": "app org.kde.plasma-systemmonitor.desktop System/",
    "shell/menus/tests/expected-fr.txt": "app org.kde.plasma-systemmonitor.desktop System/",
}


def files():
    out = []
    for top in SCANNED:
        for base, dirs, names in os.walk(os.path.join(ROOT, top)):
            dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
            for n in names:
                if n.endswith(SKIP_SUFFIX):
                    continue
                out.append(os.path.join(base, n))
    return out


def mentions(path):
    """Each occurrence of the KDE name, with the lines around it."""
    try:
        with open(path, encoding="utf-8") as f:
            lines = f.read().splitlines()
    except (UnicodeDecodeError, OSError):
        return []
    out = []
    for i, line in enumerate(lines):
        if KDE in line:
            window = "\n".join(lines[max(0, i - 2):i + 3])
            out.append((i + 1, line.strip(), window))
    return out


def test_the_scan_really_reaches_the_call_sites():
    # the guard that this file is doing anything at all: if the scan
    # found nothing, every check below would pass on an empty list
    found = [p for p in files() if mentions(p)]
    assert len(found) >= 4, (
        "the scan found the KDE monitor named in only %d file(s) under %s."
        " Either the tree moved or this scan is broken -- every check in"
        " this file would then pass without looking at anything"
        % (len(found), ", ".join(SCANNED)))


def test_the_exemptions_are_still_true():
    broken = []
    for rel, needle in sorted(EXEMPT.items()):
        path = os.path.join(ROOT, rel)
        if not os.path.isfile(path):
            broken.append("%s is gone" % rel)
            continue
        with open(path, encoding="utf-8") as f:
            text = f.read()
        if KDE not in text:
            broken.append("%s no longer names %s: drop the exemption" % (rel, KDE))
        elif needle not in text:
            broken.append("%s no longer contains %r, which is why it was"
                          " exempt: look at it again" % (rel, needle))
    assert not broken, "; ".join(broken)



# the iso repository, checked out next to this one in /srv/raye. raye-base
# is what the image installs, so its Depends is the only place that says
# whether raye-taskmanager reaches a user's machine.
RAYE_BASE_CONTROL = os.path.join(
    os.path.dirname(ROOT), "iso", "packages", "raye-base", "debian", "control")


def test_the_shortcut_exemption_expires_when_the_package_ships():
    """Ctrl+Shift+Esc must follow what the image actually carries.

    🚨 Both ways of settling this by hand are wrong, which is why it is a
    ratchet and not an edit. Leaving the key on the KDE monitor forever
    means Raye ships its own task manager and the one key everybody presses
    opens somebody else's. Moving it now means the key does NOTHING on
    every machine, because raye-taskmanager is not installed -- the silent
    failure this whole file exists to prevent.

    So: while raye-base does not depend on raye-taskmanager, the KDE
    binding is right and this passes. The moment the dependency becomes
    real, this goes red and says which line to change.
    """
    if not os.path.isfile(RAYE_BASE_CONTROL):
        pytest.skip("the iso repository is not checked out next to this one")
    with open(RAYE_BASE_CONTROL, encoding="utf-8") as f:
        control = f.read()

    # The ratchet must not be disarmable by deletion. If the line goes
    # away entirely, nothing below would ever fire again and the exemption
    # would outlive the reason for it -- silently, which is the shape of
    # every defect in this tree.
    assert "raye-taskmanager" in control, (
        "raye-base/debian/control no longer mentions raye-taskmanager at all, "
        "so this ratchet can never fire again. Either the package was renamed "
        "-- follow it here -- or it was abandoned, and then the exemption for "
        "config/kglobalshortcutsrc has to be rewritten on purpose.")

    # a real dependency: the name on a line that is not a comment
    shipped = [l for l in control.splitlines()
               if "raye-taskmanager" in l and not l.lstrip().startswith("#")]
    shortcuts = os.path.join(ROOT, "config", "kglobalshortcutsrc")
    with open(shortcuts, encoding="utf-8") as f:
        text = f.read()

    if shipped:
        assert RAYE in text, (
            "raye-base now depends on raye-taskmanager (%s), so the image "
            "carries it -- but config/kglobalshortcutsrc still binds "
            "Ctrl+Shift+Esc to %s. Move it to %s, and drop the exemption at "
            "the top of this file." % (shipped[0].strip(), KDE, RAYE))
    else:
        assert KDE in text, (
            "raye-taskmanager is not shipped (its line in raye-base/debian/"
            "control is commented out), so Ctrl+Shift+Esc must stay on %s. "
            "As it is, the key opens nothing on every machine." % KDE)

def test_raye_s_task_manager_comes_first_everywhere_else():
    bad = []
    for path in sorted(files()):
        rel = os.path.relpath(path, ROOT)
        if rel in EXEMPT:
            continue
        for line_no, line, window in mentions(path):
            if RAYE not in window:
                bad.append("%s:%d names %s and never %s:\n    %s"
                           % (rel, line_no, KDE, RAYE, line))
            elif window.index(RAYE) > window.index(KDE):
                bad.append("%s:%d puts %s before %s, so the KDE monitor"
                           " wins even when Raye's is installed:\n    %s"
                           % (rel, line_no, KDE, RAYE, line))
    assert not bad, "\n".join(bad)


def test_the_three_menus_fall_back_rather_than_fail_shut():
    # Each of the three has to name BOTH, or it breaks on a machine
    # without one of the two packages.
    wanted = [
        ("shell/plasmoids/org.raye.clock/contents/ui/main.qml", "the clock menu"),
        ("shell/plasmoids/org.raye.taskbar/contents/ui/main.qml", "the taskbar menu"),
        ("shell/desktopmenu/src/desktopmenu.cpp", "the desktop menu"),
    ]
    bad = []
    for rel, what in wanted:
        path = os.path.join(ROOT, rel)
        assert os.path.isfile(path), "%s is gone" % rel
        with open(path, encoding="utf-8") as f:
            text = f.read()
        if RAYE not in text:
            bad.append("%s (%s) never names %s" % (rel, what, RAYE))
        if KDE not in text:
            bad.append("%s (%s) dropped the fallback on %s: the menu item"
                       " does nothing where raye-taskmanager is not"
                       " installed" % (rel, what, KDE))
    assert not bad, "\n".join(bad)
