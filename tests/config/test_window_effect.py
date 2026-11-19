# SPDX-License-Identifier: GPL-3.0-or-later
"""Windows open and close the way Windows 11 does it (raye_windows).

KWin's stock scale effect runs the fade and the zoom on one curve and one
duration, both ways. Windows fades a new window in quickly while it settles
from a little smaller, and lets a closed one go faster than it came. The
effect's logic runs in QML's JavaScript engine with a stand-in for KWin's
scripting API, as for raye_maximize.
"""
import configparser
import json
import os
import re
import shutil
import subprocess
from pathlib import Path

import pytest

TOP = Path(__file__).resolve().parents[2]
FX = TOP / "kwin/effects/raye_windows"

SCALE, OPACITY = 6, 7
CURVES = {"Linear": 0, "OutQuint": 1, "InCubic": 2, "OutCubic": 3}


def kwinrc():
    cfg = configparser.ConfigParser(strict=False, interpolation=None)
    cfg.optionxform = str
    cfg.read(TOP / "config/kwinrc")
    return cfg


def test_stock_scale_is_off_and_rayes_on():
    p = kwinrc()["Plugins"]
    assert p.get("scaleEnabled") == "false"
    assert p.get("raye_windowsEnabled") == "true"


def test_metadata():
    d = json.loads((FX / "metadata.json").read_text())
    assert d["KPackageStructure"] == "KWin/Effect"
    assert d["KPlugin"]["Id"] == "raye_windows"
    assert d["KPlugin"]["Name[fr]"] and d["KPlugin"]["Description[fr]"]
    assert "kwin/effects/raye_windows" in (TOP / "debian/raye-theme.install").read_text()


def test_kwinrc_says_what_the_tokens_say():
    want = json.loads((TOP / "tokens/raye-tokens.json").read_text())["motion"]["kwin"]["Effect-raye_windows"]
    got = kwinrc()["Effect-raye_windows"]
    assert {k: float(got[k]) for k in want} == {k: float(v) for k, v in want.items()}


HARNESS = r"""
import QtQuick
Item {
    function run() {
        var req = new XMLHttpRequest();
        req.open("GET", "file://%(js)s", false);
        req.send();
        var calls = [], cancelled = [];
        var handlers = {};
        var factor = %(factor)s;
        function signal(name) { return { connect: function (f) { handlers[name] = f; } }; }
        var effects = { hasActiveFullScreenEffect: false, windowAdded: signal("added"),
                        windowClosed: signal("closed"), windowDataChanged: signal("data") };
        var grabbed = {};
        var effect = { configChanged: signal("config"), animationEnded: signal("ended"),
                       readConfig: function (k, d) { return %(config)s[k] !== undefined ? %(config)s[k] : d; },
                       isGrabbed: function (w, role) { return !!w.grabbedBy; },
                       grab: function () { return true; } };
        var Effect = { Scale: 6, Opacity: 7, WindowAddedGrabRole: 1, WindowClosedGrabRole: 2,
                       WindowForceBlurRole: 3, WindowForceBackgroundContrastRole: 4 };
        var QEasingCurve = { Linear: 0, OutQuint: 1, InCubic: 2, OutCubic: 3 };
        function animate(s) { calls.push(s); return calls.length; }
        function animationTime(ms) { return ms * factor; }
        function cancel(id) { cancelled.push(id); }
        new Function("effects", "effect", "Effect", "QEasingCurve", "animate", "animationTime", "cancel",
                     req.responseText)(effects, effect, Effect, QEasingCurve, animate, animationTime, cancel);
        function win(extra) {
            var w = { visible: true, hasDecoration: true, managed: true, normalWindow: true,
                      windowClass: "org.raye.files org.raye.files", data: {},
                      setData: function (role, v) { this.data[role] = v; } };
            for (var k in extra) w[k] = extra[k];
            return w;
        }
        var out = {};
        var a = win({});
        handlers.added(a);
        out.open = calls.slice();
        out.blurWhileOpening = a.data[3] === true;
        calls.length = 0;
        handlers.closed(a);
        out.close = calls.slice();
        out.cancelledOpen = cancelled.length;
        calls.length = 0;
        // what Windows does not animate either
        handlers.added(win({ hasDecoration: false, popupWindow: true }));
        handlers.added(win({ hasDecoration: false, outline: true, normalWindow: false }));
        handlers.added(win({ windowClass: "plasmashell plasmashell", hasDecoration: false }));
        handlers.added(win({ windowClass: "ksplashqml ksplashqml" }));
        handlers.added(win({ visible: false }));
        out.others = calls.length;
        // a closing window another effect took over is left to it
        handlers.closed(win({ grabbedBy: "squash" }));
        out.grabbed = calls.length;
        return JSON.stringify(out);
    }
    Component.onCompleted: { console.log("RESULT " + run()); Qt.quit(); }
}
"""


def run_effect(tmp_path, factor=1, config=None):
    qml = shutil.which("qml6") or shutil.which("qml")
    if not qml:
        pytest.fail("qml6 is needed to run the effect")
    f = tmp_path / "fx.qml"
    f.write_text(HARNESS % {"js": FX / "contents/code/main.js", "factor": factor,
                            "config": json.dumps(config or {})})
    env = dict(os.environ, QT_QPA_PLATFORM="offscreen", QML_XHR_ALLOW_FILE_READ="1")
    p = subprocess.run([qml, str(f)], capture_output=True, text=True, env=env, timeout=60)
    m = re.search(r"RESULT (\{.*\})", p.stdout + p.stderr)
    assert m, p.stdout + p.stderr
    return json.loads(m.group(1))


def parts(call):
    out = {}
    for a in call["animations"]:
        out[a["type"]] = {k: a.get(k, call.get(k)) for k in ("from", "to", "duration", "curve")}
    return out


def test_open_settles_in_while_it_fades_in_fast(tmp_path):
    r = run_effect(tmp_path)
    assert len(r["open"]) == 1
    p = parts(r["open"][0])
    assert p[SCALE]["from"] == 0.94 and p[SCALE]["duration"] == 250
    assert p[SCALE]["curve"] == CURVES["OutQuint"]
    assert p[OPACITY]["from"] == 0 and p[OPACITY]["duration"] == 83
    assert p[OPACITY]["curve"] == CURVES["Linear"]
    assert r["blurWhileOpening"]


def test_close_is_quicker_and_fades_straight(tmp_path):
    r = run_effect(tmp_path)
    assert len(r["close"]) == 1
    p = parts(r["close"][0])
    assert p[SCALE]["to"] == 0.96 and p[SCALE]["duration"] == 167
    assert p[SCALE]["curve"] == CURVES["InCubic"]
    assert p[OPACITY]["to"] == 0 and p[OPACITY]["duration"] == 167
    assert p[OPACITY]["curve"] == CURVES["Linear"]
    # closed before it was done opening: the opening stops
    assert r["cancelledOpen"] == 1


def test_menus_panels_and_splash_are_left_alone(tmp_path):
    r = run_effect(tmp_path)
    assert r["others"] == 0
    assert r["grabbed"] == 0


def test_animations_off_means_no_animation(tmp_path):
    r = run_effect(tmp_path, factor=0)
    assert r["open"] == [] and r["close"] == []


def test_kwinrc_values_are_read(tmp_path):
    r = run_effect(tmp_path, config={"OpenDuration": 300, "InScale": 0.9})
    p = parts(r["open"][0])
    assert p[SCALE]["duration"] == 300 and p[SCALE]["from"] == 0.9
