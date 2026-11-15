"""The two lines of the taskbar clock: the time over the date, with room
between them.

    python3 -m pytest -q tests/shell/test_clock_lines.py

The owner's complaint -- "the date is too close to the time" -- was exact,
and the answer was a zero. ClockButton.qml stacked the two labels in a
Column with no spacing at all, and one line of Inter at 12 px measures
15 px: measured 2026-11-06 with qml6, that Column was 30 px tall for two
15 px lines, so the date sat flush against the time with nothing between
them.

WHY A TEST AND NOT JUST A NUMBER IN THE QML. The gap only reads as a gap
while the type stays small. Raise the size -- or let in a script whose
fallback font has taller metrics -- and 15 + gap + 15 stops fitting the
40 px taskbar button: the clock would then overflow the taskbar instead
of giving up its gap. clocklogic.js#lineGap clamps it, and these checks
keep both halves honest at once: the designed gap is really more than
nothing, and the stack really fits -- in the languages the owner named
(German, Arabic), and with a LONG date format, not only with "11/6/26".
Both numbers are read out of the repo (ClockButton.qml, ShellTokens.qml),
never written here, so a future change to the type size or to the button
height fails this file instead of quietly losing the gap again.

🚨 NO pytest.mark.parametrize in this file, on purpose. pytest reports an
empty parameter set as a SKIP ("got empty parameter set"), so a table
that can become empty turns into a silent pass -- a test that cannot
fail proves nothing. The cases are a plain list this file asserts is not
empty, walked inside one test, with every failure collected and named.
"""
import json
import os
import re
import shutil
import subprocess

import pytest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CLOCK = os.path.join(ROOT, "shell", "plasmoids", "org.raye.clock", "contents", "ui")
BUTTON = os.path.join(CLOCK, "ClockButton.qml")
LOGIC = os.path.join(CLOCK, "clocklogic.js")
TOKENS = os.path.join(ROOT, "shell", "lib", "qml", "ShellTokens.qml")

# The least room that still reads as room between the two lines. Below
# this the owner's complaint is back.
MIN_GAP = 2

# locale, which date format goes in the lower line, why it is here
CASES = [
    ("en_US", "ShortFormat", "what the taskbar shows by default"),
    ("fr_FR", "ShortFormat", "the second language of the distribution"),
    ("de_DE", "ShortFormat", "named by the owner"),
    ("de_DE", "LongFormat", "a long date: 'Freitag, 6. November 2026'"),
    ("ar_EG", "ShortFormat", "named by the owner; right to left, own digits"),
    ("ar_EG", "LongFormat", "a long date in Arabic"),
    ("ja_JP", "ShortFormat", "a CJK fallback font, taller metrics than Inter"),
    ("th_TH", "LongFormat", "Thai stacks marks above and below the line"),
    ("hi_IN", "LongFormat", "Devanagari, a fallback font of its own"),
]


def text(path):
    with open(path, encoding="utf-8") as f:
        return f.read()


def block(src, start):
    """The { ... } that opens at or after `start`, braces matched."""
    i = src.index("{", start)
    depth = 0
    for j in range(i, len(src)):
        if src[j] == "{":
            depth += 1
        elif src[j] == "}":
            depth -= 1
            if depth == 0:
                return src[i:j + 1]
    raise AssertionError("unbalanced braces from %d" % start)


def label(name):
    """The Text block of the clock whose objectName is `name`."""
    src = text(BUTTON)
    m = re.search(r'\bText\s*\{', src)
    while m:
        b = block(src, m.start())
        if ('objectName: "%s"' % name) in b:
            return b
        m = re.search(r'\bText\s*\{', src[m.end():])
        if m:
            m = re.match(r'[\s\S]*', "")  # unreachable, kept simple below
            break
    # simple scan: every Text block in order
    for m in re.finditer(r'\bText\s*\{', src):
        b = block(src, m.start())
        if ('objectName: "%s"' % name) in b:
            return b
    raise AssertionError('no Text with objectName "%s" in ClockButton.qml' % name)


def font_of(name):
    b = label(name)
    fam = re.search(r'font\.family:\s*"([^"]+)"', b)
    size = re.search(r'font\.pixelSize:\s*(\d+)', b)
    assert fam and size, 'no font.family/font.pixelSize on the "%s" label' % name
    return fam.group(1), int(size.group(1))


def column_block():
    """The Column that stacks the time over the date."""
    src = text(BUTTON)
    for m in re.finditer(r'\bColumn\s*\{', src):
        b = block(src, m.start())
        if 'objectName: "time"' in b and 'objectName: "date"' in b:
            return b
    raise AssertionError("ClockButton.qml no longer stacks the time over the"
                         " date in one Column; this file reads that Column")


def button_height():
    """The height the clock button really gets, through ShellTokens."""
    m = re.search(r'implicitHeight:\s*ShellTokens\.(\w+)', text(BUTTON))
    assert m, "ClockButton.qml no longer takes its height from ShellTokens"
    token = m.group(1)
    m2 = re.search(r'property\s+\w+\s+%s:\s*(\d+)' % re.escape(token), text(TOKENS))
    assert m2, "ShellTokens.%s is gone or is no longer a plain number" % token
    return int(m2.group(1))


def designed_gap():
    m = re.search(r'\bLINE_GAP\s*=\s*(\d+)', text(LOGIC))
    assert m, ("clocklogic.js no longer names the gap between the two lines"
               " (LINE_GAP); without a name it is a magic number nobody finds")
    return int(m.group(1))


# --- what the source must say ---------------------------------------------

def test_the_two_lines_are_not_flush():
    b = column_block()
    m = re.search(r'\bspacing:\s*([^\n]+)', b)
    assert m, ("the Column that stacks the time over the date sets no"
               " spacing, so the two lines touch -- that is exactly the"
               " defect: 'the date is too close to the time'")
    expr = m.group(1).strip()
    assert "lineGap" in expr, (
        "the Column's spacing is %r. It has to come from"
        " clocklogic.js#lineGap: a bare number cannot give way when the"
        " lines grow, and the clock would overflow the taskbar button"
        % expr)


def test_the_designed_gap_is_room_and_not_a_token_gesture():
    gap = designed_gap()
    assert gap >= MIN_GAP, (
        "clocklogic.js LINE_GAP is %d px. Under %d px the date is back"
        " against the time, which is what was reported." % (gap, MIN_GAP))
    assert gap + 2 * 15 <= button_height(), (
        "LINE_GAP is %d px: with two 15 px lines that is taller than the"
        " %d px taskbar button" % (gap, button_height()))


# --- what it must measure, with the real font -----------------------------

def probe(tmpdir):
    fam_t, size_t = font_of("time")
    fam_d, size_d = font_of("date")
    room = button_height()
    cases = json.dumps([[c[0], c[1]] for c in CASES])
    qml = """import QtQuick
import "%(logic)s" as Logic

Item {
    id: root
    width: 400; height: 400
    Column {
        id: col
        spacing: Logic.lineGap(t.implicitHeight, d.visible ? d.implicitHeight : 0, %(room)d)
        Text { id: t; text: "21:24"; font.family: "%(famt)s"; font.pixelSize: %(sizet)d
               font.features: { "tnum": 1 } }
        Text { id: d; text: "06/11/2026"; font.family: "%(famd)s"; font.pixelSize: %(sized)d
               font.features: { "tnum": 1 } }
    }
    Component.onCompleted: {
      try {
        var out = { room: %(room)d, designed: Logic.LINE_GAP, cases: [], clamp: {}, hidden: {} }
        var when = new Date(2026, 10, 6, 21, 24)
        var cases = %(cases)s
        for (var i = 0; i < cases.length; ++i) {
            var loc = Qt.locale(cases[i][0])
            var fmt = cases[i][1] === "LongFormat" ? Locale.LongFormat : Locale.ShortFormat
            d.text = loc.toString(when, loc.dateFormat(fmt))
            t.text = Logic.timeText(21, 24, 0, true, false)
            // a Positioner lays out on the next polish, not on assignment:
            // without this the heights read back are the previous case's
            col.forceLayout()
            out.cases.push({ locale: cases[i][0], format: cases[i][1], text: d.text,
                             timeH: t.implicitHeight, dateH: d.implicitHeight,
                             spacing: col.spacing, columnH: col.height })
        }
        d.text = "06/11/2026"
        // the gap has to give way when the lines no longer fit
        out.clamp = { roomy: Logic.lineGap(15, 15, 40), tight: Logic.lineGap(19, 19, 40),
                      none: Logic.lineGap(30, 30, 40), silly: Logic.lineGap(100, 100, 40),
                      unmeasured: Logic.lineGap(0, 0, 40), noRoom: Logic.lineGap(15, 15, 0) }
        // "Show the date" off: one line, and no gap left hanging under it
        d.visible = false
        col.forceLayout()
        out.hidden = { spacing: col.spacing, columnH: col.height, lineH: t.implicitHeight }
        console.log("RAYE_PROBE " + JSON.stringify(out))
      } catch (e) {
        console.log("RAYE_PROBE_ERROR " + e)
      }
      Qt.exit(0)
    }
}
""" % dict(logic=LOGIC, room=room, famt=fam_t, sizet=size_t, famd=fam_d, sized=size_d,
           cases=cases)
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
def measured(tmp_path_factory):
    if shutil.which("qml6") is None:
        pytest.skip("qml6 is not installed: nothing can be measured")
    return probe(tmp_path_factory.mktemp("clocklines"))


def test_the_case_list_is_not_empty():
    # see the file header: an empty table is a skip, and a skip reads
    # as cover. This is the guard that the table below really ran.
    assert len(CASES) >= 5, "the locale table of this file has been emptied"
    assert any(f == "LongFormat" for _, f, _ in CASES), \
        "no long date format left in the table; a long format is the case" \
        " a gap tuned on '11/6/26' breaks on"


def test_lineGap_gives_way_before_the_clock_overflows(measured):
    c = measured["clamp"]
    bad = []
    if c["roomy"] != measured["designed"]:
        bad.append("two 15 px lines in 40 px must keep the designed %d px, got %d"
                   % (measured["designed"], c["roomy"]))
    if c["tight"] != 2:
        bad.append("two 19 px lines in 40 px leave 2 px, got %d" % c["tight"])
    if c["none"] != 0:
        bad.append("two 30 px lines in 40 px leave nothing, got %d" % c["none"])
    if c["silly"] != 0:
        bad.append("lines taller than the button must never give a negative"
                   " gap, got %d" % c["silly"])
    if c["unmeasured"] != measured["designed"]:
        bad.append("nothing measured yet (a Text is 0 px before its first"
                   " layout): keep the designed gap, got %d" % c["unmeasured"])
    if c["noRoom"] != measured["designed"]:
        bad.append("no room reported: keep the designed gap rather than"
                   " collapse to 0, got %d" % c["noRoom"])
    assert not bad, "; ".join(bad)


def test_the_date_keeps_its_room_in_every_language(measured):
    room = measured["room"]
    bad = []
    for case in measured["cases"]:
        why = dict((c[0] + "/" + c[1], c[2]) for c in CASES)
        tag = "%s %s (%s): %r" % (case["locale"], case["format"],
                                  why.get(case["locale"] + "/" + case["format"], ""),
                                  case["text"])
        if case["spacing"] < MIN_GAP:
            bad.append("%s -- only %d px between the time and the date"
                       % (tag, case["spacing"]))
        measured_gap = case["columnH"] - case["timeH"] - case["dateH"]
        if measured_gap != case["spacing"]:
            bad.append("%s -- the Column is %d px tall, which is %d px of gap,"
                       " not the %d px asked for"
                       % (tag, case["columnH"], measured_gap, case["spacing"]))
        if case["columnH"] > room:
            bad.append("%s -- the two lines and their gap are %d px, taller"
                       " than the %d px taskbar button: the clock overflows"
                       " the taskbar" % (tag, case["columnH"], room))
    assert not bad, "\n".join(bad)


def test_one_line_leaves_no_gap_hanging(measured):
    # "Show the date" off. Column skips an invisible child AND its
    # spacing; if that ever stops being true the clock sits off-centre.
    h = measured["hidden"]
    assert h["columnH"] == h["lineH"], (
        "with the date hidden the stack is %d px for a %d px line: the gap"
        " is still being counted" % (h["columnH"], h["lineH"]))
