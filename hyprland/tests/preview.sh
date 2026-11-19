#!/bin/sh
# Not a test: a picture of raye-hypr-shell, for looking at it without a
# GPU. Hyprland cannot run without one, so the shell runs on sway's
# headless backend (wlroots, software rendering, layer-shell like
# Hyprland) against fake-hyprland.py, and grim takes the picture.
#
#   preview.sh BUILD OUT.png [start|search|quick|none] [clients.json] [app...]
#
# BUILD is the build folder of this tree. QML_IMPORT_PATH must give
# org.raye.shell and org.raye.ui (the shell and qml trees of the theme),
# and LANG the language. PREVIEW_AFTER is run on the shell's session bus
# before the picture (notify-send, for a toast). PREVIEW_APPLETSRC is a
# Plasma desktop settings file for the wallpaper to follow. Needs sway,
# grim and dbus-run-session.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
top=$(cd "$here/../.." && pwd)
build=$1
out=$2
open=${3:-none}
clients=${4:-$here/preview-clients.json}
if [ $# -ge 4 ]; then shift 4; else shift $#; fi

rt=$(mktemp -d)
chmod 700 "$rt"
share=$rt/share
mkdir -p "$share/wallpapers" "$share/locale"
ln -s "$top/wallpaper/Raye" "$share/wallpapers/Raye"
# the Raye eye, the Raye defaults (rayeshellrc) and an icon theme that is
# there without raye-icon-theme installed
for n in 16 22 24 32 48 64 128 256; do
    mkdir -p "$share/icons/hicolor/${n}x${n}/apps"
    ln -s "$top/assets/icons/raye-eye-$n.png" "$share/icons/hicolor/${n}x${n}/apps/raye-eye.png"
done
mkdir -p "$rt/xdg"
ln -s "$top/shell/lib/rayeshellrc" "$rt/xdg/rayeshellrc"
printf '[Icons]\nTheme=%s\n' "${PREVIEW_ICONS:-Papirus-Dark}" > "$rt/xdg/kdeglobals"
export XDG_CONFIG_DIRS=$rt/xdg:/etc/xdg
export XDG_CONFIG_HOME=$rt/config
mkdir -p "$XDG_CONFIG_HOME"
[ -n "${PREVIEW_APPLETSRC:-}" ] && cp "$PREVIEW_APPLETSRC" "$XDG_CONFIG_HOME/plasma-org.kde.plasma.desktop-appletsrc"
for d in "$build"/locale "${RAYE_SHELL_BUILD:-}"/locale; do
    [ -d "$d" ] && cp -r "$d"/. "$share/locale/"
done

export XDG_RUNTIME_DIR=$rt
export XDG_DATA_DIRS=$share:/usr/local/share:/usr/share
export WLR_BACKENDS=headless WLR_RENDERER=pixman WLR_LIBINPUT_NO_DEVICES=1
export RAYE_HYPR_SOCKET_DIR=$rt/hypr
export RAYE_PLASMOIDS_DIR=$top/shell/plasmoids
export XDG_MENU_PREFIX=raye-
export QT_QPA_PLATFORM=wayland QT_QUICK_BACKEND=software QT_QPA_PLATFORMTHEME=kde

cat > "$rt/sway.conf" <<EOF
output HEADLESS-1 resolution ${FAKE_WIDTH:-1280}x${FAKE_HEIGHT:-800} bg #0e0a17 solid_color
default_border pixel 1
default_floating_border pixel 1
# as raye-float does on Hyprland
for_window [app_id=".*"] floating enable, move position center
EOF

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

python3 -I "$here/fake-hyprland.py" "$rt/hypr" "$clients" > "$rt/fake.log" 2>&1 &
pids="$pids $!"
sleep 0.5

# the apps start on the shell's session bus, as they would in the session
: > "$rt/apps.sh"
for app in "$@"; do
    printf '%s > /dev/null 2>&1 &\n' "$app" >> "$rt/apps.sh"
done

dbus-run-session -- sh -c "
    '$build/bin/raye-hypr-shell' > '$rt/shell.log' 2>&1 &
    . '$rt/apps.sh'
    sleep ${PREVIEW_WAIT:-6}
    [ '$open' = none ] || '$build/bin/raye-hypr-shell' --toggle '$open' >> '$rt/shell.log' 2>&1
    ${PREVIEW_AFTER:-true}
    sleep 3
    grim '$out'
    kill \$(jobs -p) 2>/dev/null
" || true
cp "$rt/shell.log" "$out.log" 2>/dev/null || true
