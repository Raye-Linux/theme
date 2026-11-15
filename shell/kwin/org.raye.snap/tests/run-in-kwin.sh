#!/bin/sh
# Raye Snap in a real KWin: a nested, virtual kwin_wayland (no screen
# needed), one window, then the script's shortcuts invoked through
# kglobalaccel. A second, tiny script reports where the window ends up.
# By hand, not by ctest (it needs a session bus and a working kwin_wayland):
#
#   dbus-run-session sh run-in-kwin.sh <theme dir>
#
# Prints one line per step and "all good" at the end; exits 1 on the
# first step that does not give the expected frame.
set -u
THEME=$(cd "${1:?theme repo}" && pwd)
PACKAGE=$THEME/shell/kwin/org.raye.snap/package
WORK=$(mktemp -d)
trap 'kill $KW $APP 2>/dev/null; rm -rf "$WORK"' EXIT

export XDG_RUNTIME_DIR=$WORK/run HOME=$WORK/home
mkdir -p "$XDG_RUNTIME_DIR" "$HOME"
chmod 700 "$XDG_RUNTIME_DIR"
export QML_IMPORT_PATH=$THEME/qml

kwin_wayland --virtual --no-lockscreen --socket raye-snap-test --width 1280 --height 800 >"$WORK/kwin.log" 2>&1 &
KW=$!
sleep 4

# a window to move about
cat >"$WORK/window.qml" <<'EOF'
import QtQuick
import QtQuick.Window
Window { visible: true; width: 600; height: 400; title: "Snap test"; color: "#2b2340" }
EOF
WAYLAND_DISPLAY=raye-snap-test QT_QPA_PLATFORM=wayland qml6 "$WORK/window.qml" >/dev/null 2>&1 &
APP=$!
sleep 3

# the reporter: the frame of the test window, on demand
mkdir -p "$WORK/report"
cat >"$WORK/report/main.qml" <<'EOF'
import QtQuick
import org.kde.kwin
Item {
    ShortcutHandler {
        name: "Raye Snap Test Report"
        text: "report"
        onActivated: {
            for (const w of Workspace.windows) {
                if (w.caption === "Snap test") {
                    const f = w.frameGeometry;
                    console.log("FRAME", Math.round(f.x), Math.round(f.y), Math.round(f.width), Math.round(f.height),
                                "MAX", w.maximizeMode, "MIN", w.minimized);
                }
            }
        }
    }
    ShortcutHandler {
        name: "Raye Snap Test Activate"
        text: "activate"
        onActivated: {
            for (const w of Workspace.windows) {
                if (w.caption === "Snap test") {
                    w.minimized = false;
                    Workspace.activeWindow = w;
                }
            }
        }
    }
}
EOF

load() {
    id=$(qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.loadDeclarativeScript "$1" "$2")
    qdbus6 org.kde.KWin "/Scripting/Script$id" org.kde.kwin.Script.run
}
invoke() {
    qdbus6 org.kde.kglobalaccel /component/kwin org.kde.kglobalaccel.Component.invokeShortcut "$1" >/dev/null
    sleep 1
}
frame() {
    invoke "Raye Snap Test Report"
    grep FRAME "$WORK/kwin.log" | tail -1 | sed 's/.*FRAME //'
}
step() {
    invoke "$1"
    got=$(frame)
    case "$got" in
        "$2"*) echo "ok   $1 -> $got" ;;
        *) echo "FAIL $1 -> $got (wanted $2)"; grep -i 'error\|org.raye.snap' "$WORK/kwin.log" | tail -5; exit 1 ;;
    esac
}

load "$PACKAGE/contents/ui/main.qml" org.raye.snap
load "$WORK/report/main.qml" rayesnaptestreport
sleep 1
invoke "Raye Snap Test Activate"
start=$(frame)
echo "start $start"

# the virtual screen is 1280 x 800 with no panel
step "Raye Snap Left" "0 0 640 800"
step "Raye Snap Up" "0 0 640 400"
step "Raye Snap Down" "0 0 640 800"
step "Raye Snap Left" "640 0 640 800"
step "Raye Snap Left" "${start%% MAX*}"
step "Raye Snap Up" "0 0 1280 800 MAX 3"
step "Raye Snap Down" "${start%% MAX*} MAX 0"
step "Raye Snap Full Height" "$(echo "$start" | awk '{print $1}') 0 $(echo "$start" | awk '{print $3}') 800"
step "Raye Snap Down" "$(echo "$start" | awk '{print $1}') 0 $(echo "$start" | awk '{print $3}') 800 MAX 0 MIN true"
invoke "Raye Snap Test Activate"

# the layouts flyout opens without an error in KWin's log
invoke "Raye Snap Layouts"
if grep -E 'org.raye.snap.*(Error|error|TypeError|ReferenceError)' "$WORK/kwin.log"; then
    echo "FAIL errors from the script"
    exit 1
fi
echo "ok   Raye Snap Layouts (no error)"
echo "all good"
