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
    for bad in ("CrossFadePrevious", "Effect.Size", "Effect.Scale", "Effect.Translation"):
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
        var Effect = { Clip: 9 };
        var QEasingCurve = { OutCubic: 7 };
        function animate(s) { calls.push(s); return calls.length; }
        function animationTime(ms) { return ms; }
        function cancel() {}
        new Function("effects", "effect", "Effect", "QEasingCurve", "animate", "animationTime", "cancel",
                     req.responseText)(effects, effect, Effect, QEasingCurve, animate, animationTime, cancel);
        function win(g) {
            var w = { visible: true, geometry: g, h: {} };
            ["windowMaximizedStateAboutToChange", "windowMaximizedStateChanged", "windowFrameGeometryChanged"]
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
        // restored: no animation
        calls.length = 0;
        a.h.windowMaximizedStateAboutToChange(a);
        a.geometry = { x: 560, y: 216, width: 800, height: 600 };
        a.h.windowMaximizedStateChanged(a);
        out.restore = calls.length;
        // the new size arrives after the state: it waits for it
        var b = win({ x: 0, y: 0, width: 960, height: 1032 });
        b.h.windowMaximizedStateAboutToChange(b);
        b.h.windowMaximizedStateChanged(b);
        out.early = calls.length;
        b.geometry = { x: 0, y: 0, width: 1920, height: 1032 };
        b.h.windowFrameGeometryChanged(b);
        out.late = calls.slice();
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
    anim = r["max"][0]["animations"]
    assert len(anim) == 1 and anim[0]["type"] == 9
    assert abs(anim[0]["from"]["value1"] - 800 / 1920) < 1e-6
    assert abs(anim[0]["from"]["value2"] - 600 / 1032) < 1e-6
    assert anim[0]["to"] == {"value1": 1.0, "value2": 1.0}
    # it was in the middle: it opens from the middle
    assert anim[0]["sourceAnchor"] == 0 == anim[0]["targetAnchor"]
    assert r["restore"] == 0


def test_a_late_size_is_waited_for(tmp_path):
    r = run_effect(tmp_path)
    assert r["early"] == 0
    late = r["late"][-1]["animations"][0]
    # the left half grows to the right: anchored left and top
    assert late["sourceAnchor"] == 1 | 2
    assert abs(late["from"]["value1"] - 0.5) < 1e-6
