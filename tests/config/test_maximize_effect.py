# SPDX-License-Identifier: GPL-3.0-or-later
"""Maximising draws the real window only, never a stretched picture.

KWin's stock maximise effect fades a picture of the window at its old size,
stretched to the new one, into the real window: soft, doubled and off its
place for 250 ms. config/kwinrc turns it off for raye_maximize, which only
clips the real window. The effect's logic is run in QML's JavaScript engine
with a stand-in for KWin's scripting API.
"""
import json
import os
import re
import shutil
import subprocess
from pathlib import Path

import pytest

TOP = Path(__file__).resolve().parents[2]
FX = TOP / "kwin/effects/raye_maximize"


def kwinrc_plugins():
    out, inside = {}, False
    for line in (TOP / "config/kwinrc").read_text().splitlines():
        line = line.strip()
        if line.startswith("["):
            inside = line == "[Plugins]"
        elif inside and "=" in line and not line.startswith("#"):
            k, _, v = line.partition("=")
            out[k] = v.strip()
    return out


def test_stock_maximize_is_off_and_rayes_on():
    p = kwinrc_plugins()
    assert p.get("maximizeEnabled") == "false"
    assert p.get("raye_maximizeEnabled") == "true"


def test_metadata():
    d = json.loads((FX / "metadata.json").read_text())
    assert d["KPackageStructure"] == "KWin/Effect"
    assert d["KPlugin"]["Id"] == "raye_maximize"
    assert d["KPlugin"]["Name[fr]"] and d["KPlugin"]["Description[fr]"]
    assert "kwin/effects/raye_maximize" in (TOP / "debian/raye-theme.install").read_text()


def test_nothing_is_scaled_or_crossfaded():
    js = (FX / "contents/code/main.js").read_text()
    code = "\n".join(l for l in js.splitlines() if not l.strip().startswith(("//", "*", "/*")))
    for bad in ("CrossFadePrevious", "Effect.Size", "Effect.Scale"):
        assert bad not in code, bad


HARNESS = r"""
import QtQuick
Item {
    function run() {
        var req = new XMLHttpRequest();
        req.open("GET", "file://%(js)s", false);
        req.send();
        var calls = [];
        var handlers = {};
        function signal(name) { return { connect: function (f) { handlers[name] = f; } }; }
        var effects = { stackingOrder: [], windowAdded: signal("added") };
        var effect = { configChanged: signal("config") };
        var Effect = { Clip: 9, Translation: 4, Opacity: 7 };
        var QEasingCurve = { OutCubic: 7, OutQuint: 8, Linear: 0 };
        function animate(s) { calls.push(s); return calls.length; }
        function animationTime(ms) { return ms; }
        function cancel() {}
        var now = 1000;
        var clock = { now: function () { return now; } };
        new Function("effects", "effect", "Effect", "QEasingCurve", "animate", "animationTime", "cancel", "Date",
                     req.responseText)(effects, effect, Effect, QEasingCurve, animate, animationTime, cancel, clock);
        function win(g) {
            var w = { visible: true, geometry: g, h: {} };
            ["windowMaximizedStateAboutToChange", "windowMaximizedStateChanged", "windowFrameGeometryChanged",
             "windowFinishUserMovedResized"]
                .forEach(function (n) { w[n] = { connect: function (f) { w.h[n] = f; } }; });
            handlers.added(w);
            return w;
        }
        var out = {};
        // centred 800x600 on 1920x1032, maximised
        var a = win({ x: 560, y: 216, width: 800, height: 600 });
        a.h.windowMaximizedStateAboutToChange(a);
        a.geometry = { x: 0, y: 0, width: 1920, height: 1032 };
        a.h.windowMaximizedStateChanged(a);
        out.max = calls.slice();
        // restored: it settles into its place, never scaled
        calls.length = 0;
        a.h.windowMaximizedStateAboutToChange(a);
        a.geometry = { x: 560, y: 216, width: 800, height: 600 };
        a.h.windowMaximizedStateChanged(a);
        out.restore = calls.slice();
        // restored to a corner: it comes from that side
        calls.length = 0;
        var c = win({ x: 0, y: 0, width: 1920, height: 1032 });
        c.h.windowMaximizedStateAboutToChange(c);
        c.geometry = { x: 40, y: 30, width: 600, height: 400 };
        c.h.windowMaximizedStateChanged(c);
        out.corner = calls.slice();
        // pulled off the top by its title bar: it stays under the pointer
        calls.length = 0;
        var f = win({ x: 0, y: 0, width: 1920, height: 1032 });
        f.h.windowMaximizedStateAboutToChange(f);
        f.move = true;
        f.geometry = { x: 500, y: 0, width: 800, height: 600 };
        f.h.windowMaximizedStateChanged(f);
        out.dragged = calls.length;
        // dropped on the left half of the screen, from a 700x500 frame: it
        // opens out from the top left of its place, where the preview put it
        calls.length = 0;
        var d = win({ x: 600, y: 300, width: 700, height: 500 });
        d.h.windowFinishUserMovedResized(d);
        var old = d.geometry;
        d.geometry = { x: 0, y: 0, width: 960, height: 1032 };
        d.h.windowFrameGeometryChanged(d, old);
        out.drop = calls.slice();
        // a plain move, no new size: nothing
        calls.length = 0;
        var e = win({ x: 100, y: 100, width: 700, height: 500 });
        e.h.windowFinishUserMovedResized(e);
        old = e.geometry;
        e.geometry = { x: 140, y: 100, width: 700, height: 500 };
        e.h.windowFrameGeometryChanged(e, old);
        out.move = calls.length;
        // the new size arrives after the state: it waits for it
        var b = win({ x: 0, y: 0, width: 960, height: 1032 });
        b.h.windowMaximizedStateAboutToChange(b);
        b.h.windowMaximizedStateChanged(b);
        out.early = calls.length;
        b.geometry = { x: 0, y: 0, width: 1920, height: 1032 };
        b.h.windowFrameGeometryChanged(b);
        out.late = calls.slice();
        // a window that cannot get bigger: no new size comes, and a resize
        // by hand two seconds later is not taken for it
        calls.length = 0;
        var s = win({ x: 300, y: 200, width: 500, height: 400 });
        s.h.windowMaximizedStateAboutToChange(s);
        s.h.windowMaximizedStateChanged(s);
        now += 2000;
        s.resize = true;
        old = s.geometry;
        s.geometry = { x: 300, y: 200, width: 700, height: 500 };
        s.h.windowFrameGeometryChanged(s, old);
        out.stale = calls.length;
        return JSON.stringify(out);
    }
    Component.onCompleted: { console.log("RESULT " + run()); Qt.quit(); }
}
"""


def run_effect(tmp_path):
    qml = shutil.which("qml6") or shutil.which("qml")
    if not qml:
        pytest.fail("qml6 is needed to run the effect")
    f = tmp_path / "fx.qml"
    f.write_text(HARNESS % {"js": FX / "contents/code/main.js"})
    env = dict(os.environ, QT_QPA_PLATFORM="offscreen", QML_XHR_ALLOW_FILE_READ="1")
    p = subprocess.run([qml, str(f)], capture_output=True, text=True, env=env, timeout=60)
    m = re.search(r"RESULT (\{.*\})", p.stdout + p.stderr)
    assert m, p.stdout + p.stderr
    return json.loads(m.group(1))


def test_maximise_opens_out_the_real_window(tmp_path):
    r = run_effect(tmp_path)
    assert len(r["max"]) == 1
    anim = {a["type"]: a for a in r["max"][0]["animations"]}
    assert set(anim) == {9, 4}
    clip, move = anim[9], anim[4]
    # KWin clips from the new frame's top left and the window is shifted
    # back to where it was: what shows first is exactly the old frame,
    # title bar included, then each edge goes to its new place
    assert clip["sourceAnchor"] == 1 | 2 == clip["targetAnchor"]
    assert abs(clip["from"]["value1"] - (560 + 800) / 1920) < 1e-6
    assert abs(clip["from"]["value2"] - (216 + 600) / 1032) < 1e-6
    assert clip["to"] == {"value1": 1.0, "value2": 1.0}
    assert move["from"] == {"value1": 560, "value2": 216}
    assert move["to"] == {"value1": 0, "value2": 0}
    assert clip["curve"] == move["curve"]
    assert r["max"][0]["duration"] == 250


def test_restore_settles_in(tmp_path):
    r = run_effect(tmp_path)
    assert len(r["restore"]) == 1
    types = {a["type"]: a for a in r["restore"][0]["animations"]}
    # centred: nothing to travel, a quick fade from most of the way there
    assert set(types) == {7}
    assert 0.5 <= types[7]["from"] < 1
    assert len(r["corner"]) == 1
    types = {a["type"]: a for a in r["corner"][0]["animations"]}
    move = types[4]["from"]
    # it comes from the middle of the screen, a short way
    assert move["value1"] > 0 and move["value2"] > 0
    assert abs(move["value1"]) <= 64 and abs(move["value2"]) <= 64
    assert r["dragged"] == 0


def test_a_dropped_window_opens_out_in_its_zone(tmp_path):
    r = run_effect(tmp_path)
    assert len(r["drop"]) == 1
    anim = r["drop"][0]["animations"][0]
    assert anim["type"] == 9
    assert anim["sourceAnchor"] == 1 | 2 == anim["targetAnchor"]
    assert abs(anim["from"]["value1"] - 700 / 960) < 1e-6
    assert abs(anim["from"]["value2"] - 500 / 1032) < 1e-6
    assert r["move"] == 0


def test_a_late_size_is_waited_for(tmp_path):
    r = run_effect(tmp_path)
    assert r["early"] == 0
    late = r["late"][-1]["animations"][0]
    # the left half grows to the right: anchored left and top
    assert late["sourceAnchor"] == 1 | 2
    assert abs(late["from"]["value1"] - 0.5) < 1e-6


def test_a_size_that_never_came_is_forgotten(tmp_path):
    r = run_effect(tmp_path)
    assert r["stale"] == 0
