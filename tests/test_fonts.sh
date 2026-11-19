#!/bin/sh
# Checks Raye's fonts, GTK defaults and KWin motion (theming TH-14):
#
#  - config/fonts/50-raye-fonts.conf: sans-serif and system-ui give
#    Inter, monospace gives Cascadia Mono, Windows font names give Inter
#    or Cascadia Mono, Arial/Times/Courier keep Liberation, Inter Display
#    is not hinted at big sizes;
#  - config/gtk-{3,4}.0/settings.ini: the same font, icons, cursor and
#    window buttons as the KDE defaults;
#  - config/kwinrc: the motion and blur values, raye_popups in place of
#    the stock popup fade;
#  - kwin/effects/raye_popups: metadata, and the effect's logic run with
#    a stand-in for KWin (qml6, no display needed).
#
#   tests/test_fonts.sh            the files of this repo
#   tests/test_fonts.sh --system   the installed fontconfig file instead
#
# Needs fontconfig, fonts-inter, fonts-cascadia-code, kreadconfig6 and
# qml6; fonts-liberation and fonts-noto-color-emoji are checked when
# they are installed.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/.." && pwd)
conf=$repo/config/fonts/50-raye-fonts.conf
system=0
[ "${1:-}" = --system ] && system=1

work=$(mktemp -d "${TMPDIR:-/tmp}/raye-fonts.XXXXXX")
trap 'rm -r -f -- "$work"' EXIT INT TERM

failures=0
ok() { echo "ok   $*"; }
bad() { echo "FAIL $*"; failures=$((failures + 1)); }
skip() { echo "skip $*"; }

# ------------------------------------------------------------ fontconfig

# The system's configuration with this repo's file dropped into conf.d,
# so it is read in the same order as once installed.
if [ "$system" = 0 ]; then
    mkdir -p "$work/conf.d" "$work/cache"
    for f in /etc/fonts/conf.d/*.conf; do
        [ "$(basename "$f")" = 50-raye-fonts.conf ] && continue
        ln -s "$f" "$work/conf.d/"
    done
    cp "$conf" "$work/conf.d/50-raye-fonts.conf"
    sed -e "s|<include ignore_missing=\"yes\">conf.d</include>|<include ignore_missing=\"yes\">$work/conf.d</include>|" \
        /etc/fonts/fonts.conf > "$work/fonts.conf"
    grep -q "$work/conf.d" "$work/fonts.conf" || { echo "cannot point fonts.conf at the test conf.d" >&2; exit 1; }
    FONTCONFIG_FILE=$work/fonts.conf
    export FONTCONFIG_FILE
else
    [ -e /etc/fonts/conf.d/50-raye-fonts.conf ] || { echo "50-raye-fonts.conf is not installed" >&2; exit 1; }
fi
XDG_CACHE_HOME=$work/cache
export XDG_CACHE_HOME

# a broken file only shows as a warning on stderr
if fc-match sans-serif 2>"$work/fc.err" >/dev/null && ! grep -qi "fontconfig" "$work/fc.err"; then
    ok "fontconfig reads the file without a warning"
else
    bad "fontconfig complains: $(cat "$work/fc.err")"
fi

installed() { fc-list : family | tr ',' '\n' | grep -qx "$1"; }

for need in Inter "Inter Display" "Cascadia Mono"; do
    installed "$need" || { echo "font $need is not installed: cannot test" >&2; exit 1; }
done

family_of() { fc-match -f '%{family[0]}' "$1"; }

expect_family() {   # pattern, family
    got=$(family_of "$1")
    if [ "$got" = "$2" ]; then ok "\"$1\" -> $2"; else bad "\"$1\" -> $got, want $2"; fi
}

for p in sans-serif system-ui "Segoe UI" "Segoe UI Variable" "Segoe UI Variable Text" \
         Tahoma "MS Shell Dlg" "MS Shell Dlg 2" "Microsoft Sans Serif" \
         "Segoe UI Light" "Segoe UI Semibold"; do
    expect_family "$p" Inter
done
expect_family "Segoe UI Variable Display" "Inter Display"
for p in monospace Consolas "Lucida Console"; do
    expect_family "$p" "Cascadia Mono"
done

# weights Windows names as families
weight_of() { fc-match -f '%{weight}' "$1"; }
w=$(weight_of "Segoe UI Semibold")
if [ "$w" = 180 ]; then ok "Segoe UI Semibold is Inter SemiBold"; else bad "Segoe UI Semibold weight $w, want 180"; fi
w=$(weight_of "Segoe UI Light")
if [ "$w" = 50 ]; then ok "Segoe UI Light is Inter Light"; else bad "Segoe UI Light weight $w, want 50"; fi
w=$(weight_of "Segoe UI")
if [ "$w" = 80 ]; then ok "Segoe UI is Inter Regular"; else bad "Segoe UI weight $w, want 80"; fi

if installed "Liberation Sans"; then
    expect_family Arial "Liberation Sans"
    expect_family "Times New Roman" "Liberation Serif"
    expect_family "Courier New" "Liberation Mono"
else
    skip "Liberation is not installed"
fi
if installed "Noto Color Emoji"; then
    expect_family "Segoe UI Emoji" "Noto Color Emoji"
else
    skip "Noto Color Emoji is not installed"
fi

# hinting: slight for Inter, none for Inter Display from 20 px
hint_of() { fc-match -f '%{hintstyle}' "$1"; }
h=$(hint_of "Inter:pixelsize=13.33")
if [ "$h" = 1 ]; then ok "Inter 13 px keeps slight hinting"; else bad "Inter 13 px hintstyle $h, want 1"; fi
h=$(hint_of "Inter Display:pixelsize=27")
if [ "$h" = 0 ]; then ok "Inter Display 27 px is not hinted"; else bad "Inter Display 27 px hintstyle $h, want 0"; fi
h=$(hint_of "Inter Display:pixelsize=16")
if [ "$h" = 1 ]; then ok "Inter Display 16 px keeps slight hinting"; else bad "Inter Display 16 px hintstyle $h, want 1"; fi

# no DPI forced
if grep -q '"dpi"' "$conf"; then bad "the fontconfig file forces a DPI"; else ok "no DPI forced"; fi

# ------------------------------------------------------------ GTK

ini() {   # file, key
    sed -n "s/^$2=//p" "$1" | tail -n 1
}
kde() {   # group, key
    kreadconfig6 --file "$repo/config/kdeglobals" --group "$1" --key "$2"
}
kde_font=$(kde General font | cut -d, -f1,2 | tr ',' ' ')
icons=$(kde Icons Theme)
for v in 3 4; do
    f=$repo/config/gtk-$v.0/settings.ini
    [ -f "$f" ] || { bad "gtk-$v.0/settings.ini is missing"; continue; }
    head -n 20 "$f" | grep -qx '\[Settings\]' && ok "gtk-$v.0: [Settings]" || bad "gtk-$v.0: no [Settings] group"
    [ "$(ini "$f" gtk-font-name)" = "$kde_font" ] && ok "gtk-$v.0: font $kde_font" \
        || bad "gtk-$v.0: font $(ini "$f" gtk-font-name), kdeglobals says $kde_font"
    [ "$(ini "$f" gtk-icon-theme-name)" = "$icons" ] && ok "gtk-$v.0: icons $icons" \
        || bad "gtk-$v.0: icons $(ini "$f" gtk-icon-theme-name), kdeglobals says $icons"
    [ "$(ini "$f" gtk-cursor-theme-name)" = Raye ] && ok "gtk-$v.0: cursor Raye" || bad "gtk-$v.0: cursor"
    [ "$(ini "$f" gtk-cursor-theme-size)" = 24 ] && ok "gtk-$v.0: cursor 24" || bad "gtk-$v.0: cursor size"
    [ "$(ini "$f" gtk-decoration-layout)" = ":minimize,maximize,close" ] \
        && ok "gtk-$v.0: buttons on the right" || bad "gtk-$v.0: decoration layout"
    [ "$(ini "$f" gtk-application-prefer-dark-theme)" = true ] \
        && ok "gtk-$v.0: dark, like Raye Night" || bad "gtk-$v.0: not dark"
    if [ -d /usr/share/themes ] && [ -n "$(ini "$f" gtk-theme-name)" ]; then
        t=$(ini "$f" gtk-theme-name)
        if [ -d "/usr/share/themes/$t/gtk-$v.0" ]; then ok "gtk-$v.0: theme $t is installed"
        else skip "gtk-$v.0: theme $t is not installed here"; fi
    fi
done

# ------------------------------------------------------------ kwinrc

kwin() { kreadconfig6 --file "$repo/config/kwinrc" --group "$1" --key "$2"; }
expect_kwin() {   # group, key, value
    got=$(kwin "$1" "$2")
    if [ "$got" = "$3" ]; then ok "kwinrc [$1] $2=$3"; else bad "kwinrc [$1] $2=$got, want $3"; fi
}
expect_kwin Plugins raye_popupsEnabled true
expect_kwin Plugins fadingpopupsEnabled false
expect_kwin Plugins scaleEnabled false
expect_kwin Plugins raye_windowsEnabled true
expect_kwin Plugins glideEnabled false
expect_kwin Plugins magiclampEnabled false
expect_kwin Plugins squashEnabled true
expect_kwin Plugins slidingpopupsEnabled true
expect_kwin Plugins blurEnabled true
expect_kwin Plugins contrastEnabled true
# `expect_kwin Plugins desktopchangeosdEnabled false` stood here, and it
# contradicted tests/config/test_kwin_plugins.py, which FORBIDS that line.
# The key was deliberately removed from config/kwinrc in November 2026
# (KPluginMetaData already returns false when EnabledByDefault is absent,
# so the line turned off something already off while reading as though
# Raye had replaced an effect). The two tests disagreed for weeks and
# nobody saw it, because THIS suite was skipped for a missing font: the
# launcher reported `theme/fonts  the font Inter is not installed` and
# moved on. Installing the font turned the skip into a red in one run.
# The absence of the key is checked where it belongs, by
# tests/config/test_kwin_plugins.py.
expect_kwin Effect-raye_windows OpenDuration 250
expect_kwin Effect-raye_windows CloseDuration 167
expect_kwin Effect-raye_windows InScale 0.94
expect_kwin Effect-slidingpopups SlideInTime 250
expect_kwin Effect-slidingpopups SlideOutTime 167
expect_kwin Effect-blur BlurStrength 11
expect_kwin Effect-blur NoiseStrength 4
expect_kwin TabBox LayoutName raye
expect_kwin TabBox DelayTime 90
expect_kwin TabBox HighlightWindows false
# the values come from the design tokens (motion.kwin): kwinrc and the
# effect must say the same
python3 - "$repo" <<'EOF' && ok "kwinrc and raye_popups match tokens/raye-tokens.json" || bad "kwinrc or raye_popups differ from the tokens"
import configparser, json, re, sys
repo = sys.argv[1]
kw = json.load(open(f"{repo}/tokens/raye-tokens.json"))["motion"]["kwin"]
cp = configparser.ConfigParser(interpolation=None, strict=True)
cp.optionxform = str
cp.read(f"{repo}/config/kwinrc")
for group in ("Effect-raye_windows", "Effect-slidingpopups", "Effect-blur"):
    for key, value in kw[group].items():
        assert float(cp[group][key]) == float(value), (group, key)
js = open(f"{repo}/kwin/effects/raye_popups/contents/code/main.js").read()
pop = kw["raye_popups"]
for const, key in (("FADE_IN_MS", "in"), ("FADE_OUT_MS", "out"), ("SLIDE_PX", "slide")):
    assert re.search(rf"var {const} = {pop[key]};", js), const
assert f"QEasingCurve.{pop['in.curve']}" in js and f"QEasingCurve.{pop['out.curve']}" in js
EOF
# one value per key: a second line for the same key would hide the first
dups=$(grep -v '^#' "$repo/config/kwinrc" | awk -F= '/^\[/{g=$0} /=/{print g $1}' | sort | uniq -d)
[ -z "$dups" ] && ok "kwinrc: no key twice" || bad "kwinrc: keys twice: $dups"

# ------------------------------------------------------------ raye_popups

fx=$repo/kwin/effects/raye_popups
python3 - "$fx/metadata.json" <<'EOF' && ok "raye_popups: metadata" || bad "raye_popups: metadata"
import json, sys
d = json.load(open(sys.argv[1]))
k = d["KPlugin"]
assert d["KPackageStructure"] == "KWin/Effect"
assert d["X-Plasma-API"] == "javascript"
assert k["Id"] == "raye_popups"
assert k["Name[fr]"] and k["Description[fr]"]
EOF

# The effect run in QML's JavaScript engine with a stand-in for KWin's
# scripting API: what it animates, how long, and from where.
cat > "$work/effect.qml" <<EOF
import QtQuick
Item {
    function run() {
        var req = new XMLHttpRequest();
        req.open("GET", "file://$fx/contents/code/main.js", false);
        req.send();
        var src = req.responseText;
        var log = [];
        var calls = [];
        var handlers = {};
        function signal(name) { return { connect: function (f) { handlers[name] = f; } }; }
        var effects = { hasActiveFullScreenEffect: false, cursorPos: { x: 100, y: 100 },
                        windowAdded: signal("added"), windowClosed: signal("closed"),
                        windowDataChanged: signal("data") };
        var grabbed = {};
        var effect = { configChanged: signal("config"),
                       grab: function (w, r) { if (grabbed[w.name]) return false; grabbed[w.name] = r; return true; },
                       isGrabbed: function () { return false; } };
        var Effect = { Opacity: 7, Translation: 2, WindowAddedGrabRole: 1, WindowClosedGrabRole: 2 };
        var QEasingCurve = { Linear: 0, OutQuint: 14, OutQuart: 12 };
        function animate(s) { calls.push(s); return calls.length; }
        function animationTime(ms) { return ms; }
        function cancel() {}
        var api = new Function("effects", "effect", "Effect", "QEasingCurve", "animate",
                               "animationTime", "cancel", src + "\nreturn { slideOffset: slideOffset };");
        var fx = api(effects, effect, Effect, QEasingCurve, animate, animationTime, cancel);
        function check(what, cond) { log.push((cond ? "ok   " : "FAIL ") + "raye_popups: " + what); }
        function win(name, props) {
            var w = { name: name, visible: true, managed: true, windowClass: "app app",
                      frameGeometry: { x: 90, y: 104, width: 200, height: 300 } };
            for (var k in props) w[k] = props[k];
            return w;
        }

        // a menu that opens below the pointer: fade + slide down from 6 px above
        handlers.added(win("menu", { popupWindow: true, popupMenu: true }));
        var a = calls[calls.length - 1];
        check("menu fades in over 167 ms", a.duration === 167 && a.animations[0].type === Effect.Opacity
              && a.animations[0].from === 0 && a.animations[0].to === 1);
        check("menu fade uses OutQuint", a.animations[0].curve === QEasingCurve.OutQuint);
        check("menu below the pointer slides down 6 px", a.animations.length === 2
              && a.animations[1].type === Effect.Translation
              && a.animations[1].from.value2 === -6 && a.animations[1].to.value2 === 0);

        // a menu that opens above the pointer (near the bottom of the screen)
        effects.cursorPos = { x: 100, y: 500 };
        handlers.added(win("menu2", { popupWindow: true, popupMenu: true,
                                      frameGeometry: { x: 90, y: 196, width: 200, height: 300 } }));
        a = calls[calls.length - 1];
        check("menu above the pointer slides up 6 px", a.animations[1].from.value2 === 6);

        // closing: 83 ms, linear, no slide
        var n = calls.length;
        handlers.closed(win("menu3", { popupWindow: true, popupMenu: true }));
        a = calls[calls.length - 1];
        check("menu closes in 83 ms, linear", calls.length === n + 1 && a.duration === 83
              && a.type === Effect.Opacity && a.from === 1 && a.to === 0 && a.curve === QEasingCurve.Linear);

        // a notification fades without sliding
        handlers.added(win("note", { notification: true }));
        a = calls[calls.length - 1];
        check("notification fades, no slide", a.animations.length === 1);

        // a normal window, the lock screen and an already taken popup are left alone
        n = calls.length;
        handlers.added(win("normal", {}));
        handlers.added(win("lock", { popupWindow: true, windowClass: "kscreenlocker_greet kscreenlocker_greet" }));
        grabbed["taken"] = 1;
        handlers.added(win("taken", { popupWindow: true, popupMenu: true }));
        check("normal windows, lock screen and taken popups are not animated", calls.length === n);

        // a browser's address bar list (unmanaged utility) is left alone
        handlers.added(win("utility", { managed: false, utility: true }));
        check("unmanaged utility windows are not animated", calls.length === n);

        // no pointer position: fade only
        check("no slide without a pointer position",
              fx.slideOffset(win("x", { popupMenu: true }), undefined) === null);
        check("no slide for an empty window",
              fx.slideOffset(win("x", { popupMenu: true, frameGeometry: { x: 0, y: 0, width: 0, height: 0 } }),
                             { x: 0, y: 0 }) === null);

        // animations off: durations follow animationTime
        return log.join("\n");
    }
    Component.onCompleted: { console.log("\n" + run()); Qt.quit(); }
}
EOF
if QT_QPA_PLATFORM=offscreen QML_XHR_ALLOW_FILE_READ=1 qml6 "$work/effect.qml" >"$work/qml.out" 2>&1; then
    sed -n 's/^.*\(\(ok  \|FAIL\) raye_popups:\)/\1/p' "$work/qml.out" > "$work/qml.res"
    cat "$work/qml.res"
    k=$(grep -c '^ok ' "$work/qml.res" || true)
    f=$(grep -c '^FAIL' "$work/qml.res" || true)
    failures=$((failures + f))
    [ "$k" -ge 10 ] || { bad "raye_popups: the effect test did not run ($k checks)"; cat "$work/qml.out"; }
else
    bad "raye_popups: qml6 failed"; cat "$work/qml.out"
fi

echo
if [ "$failures" -gt 0 ]; then
    echo "$failures check(s) failed"
    exit 1
fi
echo "all checks passed"
