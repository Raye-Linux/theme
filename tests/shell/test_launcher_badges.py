"""The badges on the taskbar come from the applications' own unread
counters, not from the notification centre.

    python3 -m pytest -q tests/shell/test_launcher_badges.py

WHAT IT USED TO DO, measured 2026-11-06 in main.qml of org.raye.taskbar:
the badge source was a NotificationManager.Notifications model, and the
number on an icon was "how many unread notifications carry this
desktop-entry". That is not what a badge means. Clear the notification
centre and every badge vanished while the unread mail was still unread;
read a message inside the app and the badge stayed, because the
notification was still in the centre.

WHERE THEY COME FROM NOW. com.canonical.Unity.LauncherEntry on the
session bus -- the interface Telegram, Discord, Thunderbird, Nextcloud
and the GNOME/Elementary mail clients already publish. An application
emits Update(appUri, properties) with `count` and `count-visible`, and
the number on the icon is then the application's own.

🚨 THE CASE THIS FILE EXISTS FOR: A BADGE THAT GOES STALE. An
application that stops saying anything -- it quit, it crashed, it was
killed -- must LOSE its badge, not keep its last number for ever. A
badge nobody can clear sends the owner into an app to look for a
message that is not there, which is worse than no badge at all. So the
counter is dropped both ways: `count-visible: false` (or a count of 0)
clears it, and the bus name going away clears it too.

⚠️ The second trap is QML, not D-Bus: badgeCounts is a plain object
behind a property binding, so a function that EDITS it in place changes
nothing on screen. Every function here returns a new object, and that
is checked.

🚨 No pytest.mark.parametrize in this file: pytest reports an empty
parameter set as a SKIP, so a table that can empty itself becomes a
silent pass. The cases are lists walked inside one test, asserted
non-empty first.
"""
import json
import os
import re
import shutil
import subprocess

import pytest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BAR = os.path.join(ROOT, "shell", "plasmoids", "org.raye.taskbar", "contents", "ui")
MAIN = os.path.join(BAR, "main.qml")
LOGIC = os.path.join(BAR, "taskbarlogic.js")

# appUri as the interface spells it -> the app key the taskbar matches on
KEYS = [
    ("application://org.kde.kmail2.desktop", "org.kde.kmail2"),
    ("application://org.telegram.desktop.desktop", "org.telegram.desktop"),
    ("application://Thunderbird.desktop", "thunderbird"),
    ("application:///usr/share/applications/org.kde.konsole.desktop", "org.kde.konsole"),
    # not a launcher entry, or no id in it: no badge rather than a wrong one
    ("application://", ""),
    ("application://.desktop", ""),
    ("org.kde.kmail2.desktop", ""),
    ("unity://org.kde.kmail2.desktop", ""),
    ("", ""),
]


def text(path):
    with open(path, encoding="utf-8") as f:
        return f.read()


def badge_source():
    """The block of main.qml that produces badgeCounts."""
    src = text(MAIN)
    m = re.search(r'# *--- *badges|// *--- *badges', src)
    assert m, "main.qml no longer has a badges section; this file reads it"
    end = src.find("\n    // ---", m.end())
    return src[m.start():end if end > 0 else len(src)]


# --- where the numbers come from ------------------------------------------

def test_badges_are_not_notification_counts_any_more():
    block = badge_source()
    assert "NotificationManager" not in block, (
        "the taskbar still counts notifications for its badges. A badge is"
        " the application's own unread count: clearing the notification"
        " centre must not clear the mail that is still unread, and reading"
        " a message in the app must clear the badge even though the"
        " notification is still in the centre.\n--- the block ---\n" + block)


def test_badges_come_from_the_launcher_entry_interface():
    block = badge_source()
    assert "LauncherEntry" in block, (
        "nothing in the badges section of main.qml reads"
        " com.canonical.Unity.LauncherEntry, which is where an"
        " application's own count is published")
    assert "ShellSettings.badges" in block, (
        '"Show badges on taskbar buttons" no longer gates the source;'
        " the option has to switch the watcher off, not just hide the"
        " circles")


def test_a_gone_application_loses_its_badge_in_the_qml_too():
    block = badge_source()
    assert re.search(r'dropLauncherEntry|entryGone|serviceGone', block), (
        "main.qml never handles an application going away, so its last"
        " count stays on the icon for ever -- the stale badge this whole"
        " change is about")


# --- the decisions, run for real ------------------------------------------

def probe(tmpdir):
    qml = """import QtQuick
import "%(logic)s" as Logic

Item {
    Component.onCompleted: {
      try {
        var out = { keys: [], steps: [], fresh: {} }
        var keys = %(keys)s
        for (var i = 0; i < keys.length; ++i)
            out.keys.push([keys[i][0], Logic.launcherEntryKey(keys[i][0])])

        var mail = "application://org.kde.kmail2.desktop"
        var chat = "application://org.telegram.desktop.desktop"
        var counts = {}
        function step(name, next) {
            counts = next
            out.steps.push({ step: name, counts: counts })
        }
        step("mail says 5", Logic.applyLauncherEntry(counts, mail, { "count": 5, "count-visible": true }))
        step("chat says 2", Logic.applyLauncherEntry(counts, chat, { "count": 2, "count-visible": true }))
        step("mail says 7", Logic.applyLauncherEntry(counts, mail, { "count": 7, "count-visible": true }))
        step("mail hides it", Logic.applyLauncherEntry(counts, mail, { "count": 7, "count-visible": false }))
        step("mail says 4 again", Logic.applyLauncherEntry(counts, mail, { "count": 4, "count-visible": true }))
        step("mail says 0", Logic.applyLauncherEntry(counts, mail, { "count": 0, "count-visible": true }))
        step("mail says 3", Logic.applyLauncherEntry(counts, mail, { "count": 3, "count-visible": true }))
        // a progress-only update must not invent a count
        step("chat sends progress only", Logic.applyLauncherEntry(counts, chat, { "progress": 0.5, "progress-visible": true }))
        // the stale badge: the app is gone and says nothing more
        step("mail quits", Logic.dropLauncherEntry(counts, mail))
        step("mail quits twice", Logic.dropLauncherEntry(counts, mail))
        step("an unknown app quits", Logic.dropLauncherEntry(counts, "application://nobody.desktop"))
        // rubbish on the bus changes nothing
        step("a bad uri", Logic.applyLauncherEntry(counts, "application://", { "count": 9, "count-visible": true }))
        step("no properties", Logic.applyLauncherEntry(counts, chat, null))

        // a new object every time, or the QML binding never fires
        var before = { "org.kde.kmail2": 5 }
        var after = Logic.applyLauncherEntry(before, mail, { "count": 6, "count-visible": true })
        var dropped = Logic.dropLauncherEntry(before, mail)
        out.fresh = { sameApply: after === before, beforeAfterApply: before["org.kde.kmail2"],
                      sameDrop: dropped === before, beforeAfterDrop: before["org.kde.kmail2"] }
        console.log("RAYE_PROBE " + JSON.stringify(out))
      } catch (e) {
        console.log("RAYE_PROBE_ERROR " + e)
      }
      Qt.exit(0)
    }
}
""" % dict(logic=LOGIC, keys=json.dumps([[k, v] for k, v in KEYS]))
    path = os.path.join(str(tmpdir), "probe.qml")
    with open(path, "w", encoding="utf-8") as f:
        f.write(qml)
    env = dict(os.environ, QT_QPA_PLATFORM="offscreen", QT_QUICK_BACKEND="software",
               QT_LOGGING_RULES="qt.text.font.db=false")
    try:
        r = subprocess.run(["qml6", path], capture_output=True, text=True,
                           timeout=60, env=env)
    except subprocess.TimeoutExpired:
        raise AssertionError("qml6 never came back on %s" % path)
    for line in (r.stdout + r.stderr).splitlines():
        if "RAYE_PROBE_ERROR " in line:
            raise AssertionError(line.split("RAYE_PROBE_ERROR ", 1)[1])
        if "RAYE_PROBE " in line:
            return json.loads(line.split("RAYE_PROBE ", 1)[1])
    raise AssertionError("the probe printed nothing usable.\nstdout:\n%s\nstderr:\n%s"
                         % (r.stdout, r.stderr))


@pytest.fixture(scope="module")
def ran(tmp_path_factory):
    if shutil.which("qml6") is None:
        pytest.skip("qml6 is not installed: the decisions cannot be run")
    return probe(tmp_path_factory.mktemp("badges"))


def test_the_tables_are_not_empty():
    # see the header: an empty table is a skip, and a skip reads as cover
    assert len(KEYS) >= 5, "the appUri table of this file has been emptied"
    assert any(v == "" for _, v in KEYS), "no rejected appUri left in the table"


def test_an_app_uri_becomes_the_key_the_taskbar_matches_on(ran):
    got = dict((uri, key) for uri, key in ran["keys"])
    bad = []
    for uri, want in KEYS:
        if got.get(uri) != want:
            bad.append("%r -> %r, expected %r" % (uri, got.get(uri), want))
    assert not bad, "; ".join(bad)


def test_the_counter_follows_the_application(ran):
    steps = dict((s["step"], s["counts"]) for s in ran["steps"])
    mail, chat = "org.kde.kmail2", "org.telegram.desktop"
    bad = []

    def want(step, expected):
        if steps.get(step) != expected:
            bad.append("after %r the badges are %r, expected %r"
                       % (step, steps.get(step), expected))

    want("mail says 5", {mail: 5})
    want("chat says 2", {mail: 5, chat: 2})
    want("mail says 7", {mail: 7, chat: 2})
    # count-visible false is the app saying "no badge", not "keep the 7"
    want("mail hides it", {chat: 2})
    want("mail says 4 again", {chat: 2, mail: 4})
    # zero is not a badge either
    want("mail says 0", {chat: 2})
    want("mail says 3", {chat: 2, mail: 3})
    # progress is a different feature: it must not conjure a count
    want("chat sends progress only", {chat: 2, mail: 3})
    # the stale badge
    want("mail quits", {chat: 2})
    want("mail quits twice", {chat: 2})
    want("an unknown app quits", {chat: 2})
    want("a bad uri", {chat: 2})
    want("no properties", {chat: 2})
    assert not bad, "\n".join(bad)


def test_every_change_gives_a_new_object(ran):
    f = ran["fresh"]
    bad = []
    if f["sameApply"]:
        bad.append("applyLauncherEntry returned the object it was given:"
                   " the QML binding on badgeCounts never fires, so the"
                   " circles on the taskbar never change")
    if f["beforeAfterApply"] != 5:
        bad.append("applyLauncherEntry edited its argument in place"
                   " (5 became %r)" % f["beforeAfterApply"])
    if f["sameDrop"]:
        bad.append("dropLauncherEntry returned the object it was given")
    if f["beforeAfterDrop"] != 5:
        bad.append("dropLauncherEntry edited its argument in place"
                   " (5 became %r)" % f["beforeAfterDrop"])
    assert not bad, "\n".join(bad)
