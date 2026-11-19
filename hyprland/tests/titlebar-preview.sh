#!/bin/sh
# Not a test: the Raye title bar of a Qt app, drawn and clicked without a
# GPU. Hyprland needs one, so the app runs on sway's headless backend with
# the same Qt settings as the Raye Hyprland session, next to raye-hypr-shell
# and fake-hyprland.py, and grim takes the pictures.
#
#   titlebar-preview.sh BUILD OUTDIR [COLORS]
#
# COLORS is the colour scheme the app reads (default: Raye Night). A
# headless sway has no pointer: set VPOINTER to a small client of the
# wlr virtual pointer protocol ("vpointer W H m X Y | c | s MS ...") to
# hover and click the buttons too.
#
# Writes OUTDIR/{bar,hover-close,hover-max}.png and OUTDIR/log.txt: the
# app's own lines (state, closed), the requests the shell sent to
# Hyprland, and the app's Wayland requests (set_maximized). Needs sway,
# grim and dbus-run-session.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
top=$(cd "$here/../.." && pwd)
build=$1
out=$2
colors=${3:-$top/colors/RayeNight.colors}
mkdir -p "$out"

rt=$(mktemp -d)
chmod 700 "$rt"
mkdir -p "$rt/plugins/wayland-decoration-client" "$rt/hypr"
ln -s "$build/bin/rayetitlebar.so" "$rt/plugins/wayland-decoration-client/"
export XDG_RUNTIME_DIR=$rt
export WLR_BACKENDS=headless WLR_RENDERER=pixman WLR_LIBINPUT_NO_DEVICES=1
export RAYE_HYPR_SOCKET_DIR=$rt/hypr
export XDG_CONFIG_HOME=$rt/config
mkdir -p "$XDG_CONFIG_HOME"
cp "$colors" "$XDG_CONFIG_HOME/kdeglobals"

cat > "$rt/sway.conf" <<CONF
output HEADLESS-1 resolution 1280x800 bg #0e0a17 solid_color
default_border pixel 1
default_floating_border pixel 1
for_window [app_id=".*"] floating enable, move position center
CONF

pids=
cleanup() {
    for p in $pids; do kill "$p" 2>/dev/null || true; done
    rm -rf "$rt"
}
trap cleanup EXIT

sway -c "$rt/sway.conf" > "$rt/sway.log" 2>&1 &
pids="$pids $!"
for _ in 1 2 3 4 5 6 7 8 9 10; do
    [ -S "$rt/wayland-1" ] && break
    sleep 0.5
done
export WAYLAND_DISPLAY=wayland-1

echo '[]' > "$rt/clients.json"
python3 -I "$here/fake-hyprland.py" "$rt/hypr" "$rt/clients.json" > "$rt/fake.log" 2>&1 &
pids="$pids $!"

# where the bar is: the 640 x 400 window centred on 1280 x 800, plus its bar
left=320
top=$(( (800 - 432) / 2 ))
right=$(( left + 640 ))

dbus-run-session -- sh -c "
    QT_QPA_PLATFORM=wayland QT_QUICK_BACKEND=software QT_QPA_PLATFORMTHEME=kde \
    QT_PLUGIN_PATH='$rt/plugins' QT_WAYLAND_DECORATION=raye \
    QT_WAYLAND_DISABLED_INTERFACES=zxdg_decoration_manager_v1,org_kde_kwin_server_decoration_manager \
    WAYLAND_DEBUG=client qml6 '$here/titlebar-app.qml' -- '${PREVIEW_TITLE:-Documents}' > '$rt/app.log' 2>&1 &
    app=\$!
    sleep 4
    printf '[{\"address\":\"0xa\",\"pid\":%s,\"title\":\"%s\",\"class\":\"qml6\",\"mapped\":true,\"hidden\":false,\"workspace\":{\"id\":1,\"name\":\"1\"},\"focusHistoryID\":0}]' \"\$app\" '${PREVIEW_TITLE:-Documents}' > '$rt/clients.json'
    '$build/bin/raye-hypr-shell' --qml '$here/titlebar-shell.qml' > '$rt/shell.log' 2>&1 &
    sleep 1
    grim '$out/bar.png'
    if [ -n '${VPOINTER:-}' ]; then
        '${VPOINTER:-}' 1280 800 m $(( right - 23 )) $(( top + 16 )) s 1500 m $(( right - 69 )) $(( top + 16 )) s 1500 \
            c s 1000 m $(( right - 115 )) $(( top + 16 )) s 300 c s 1000 m $(( right - 23 )) $(( top + 16 )) s 300 c s 500 &
        sleep 1; grim '$out/hover-close.png'
        sleep 1.5; grim '$out/hover-max.png'
        sleep 4
    fi
    kill \$(jobs -p) 2>/dev/null
" || true

{
    echo "== app"
    grep -v '^\[' "$rt/app.log" | grep -v '^ *->' || true
    echo "== app wayland requests"
    grep -E 'set_maximized|unset_maximized|set_minimized|zxdg_decoration|move\(' "$rt/app.log" | sed 's/^\[[^]]*\] *//' | sort | uniq -c || true
    echo "== shell to hyprland"
    grep 'dispatch' "$rt/fake.log" || true
    echo "== shell"
    cat "$rt/shell.log"
} > "$out/log.txt"
cp "$rt/app.log" "$out/app-wayland.log"
