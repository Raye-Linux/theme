#!/bin/sh
# One raye-hypr-shell per session: a second one started while the first is
# still loading must stop at once. Owner's Dell, 2026-11-21: two Raye
# taskbars stacked at the bottom of Hyprland. The shell used to ask "is the
# name taken?" and take it only once its QML had loaded, so two shells
# started close together both went on.
#   single-instance.sh /path/to/raye-hypr-shell
set -u
shell=$1
command -v dbus-run-session >/dev/null 2>&1 || { echo "SKIP: no dbus-run-session"; exit 77; }
dir=$(mktemp -d)
trap 'rm -f "$dir"/*; rmdir "$dir"' EXIT
# stands in for the shell's QML: it never asks for the D-Bus name itself
cat > "$dir/stay.qml" <<'QML'
import QtQuick
Timer { interval: 4000; running: true; onTriggered: Qt.quit() }
QML
export dir shell
dbus-run-session -- sh -c '
    QT_QPA_PLATFORM=offscreen "$shell" --qml "$dir/stay.qml" > "$dir/first.log" 2>&1 &
    first=$!
    sleep 1
    QT_QPA_PLATFORM=offscreen timeout 3 "$shell" --qml "$dir/stay.qml" > "$dir/second.log" 2>&1
    echo "second=$?" > "$dir/second.rc"
    wait "$first"
'
cat "$dir/second.rc"
if grep -q "already running" "$dir/second.log" && grep -q "second=0" "$dir/second.rc" \
    && ! grep -q "already running" "$dir/first.log"; then
    echo "PASS: the second shell stopped"
    exit 0
fi
echo "FAIL: two shells ran"
sed 's/^/first: /' "$dir/first.log"
sed 's/^/second: /' "$dir/second.log"
exit 1
