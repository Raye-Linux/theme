#!/bin/sh
# Runs the Raye lock screen in the screen locker's own greeter, in its
# testing mode, from this repo (nothing installed, nothing diverted).
#
#   tests/run-greeter.sh [greeter options...]
#
# Uses $DISPLAY (an X server, e.g. Xvfb) and $LANG. Testing mode does not
# lock anything; it still asks PAM for the password of the user running it.
#
# It builds a throw-away tree in a temporary folder:
#   share/plasma/shells/org.raye.locktest/  metadata.json from testshell/,
#       contents/lockscreen/ = a copy of the stock lock screen folder with
#       Raye's files on top, exactly as they end up once installed
#   qml/org/raye/signin/  the sign-in module, its picture paths pointed at
#       this repo instead of /usr/share
# and starts the greeter with XDG_DATA_DIRS and QML_IMPORT_PATH on it.
# The folder is removed when the greeter exits.
set -eu

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
lock=$(dirname -- "$here")
repo=$(dirname -- "$lock")

stock=/usr/share/plasma/shells/org.kde.plasma.desktop/contents/lockscreen
greeter=
for g in /usr/lib/x86_64-linux-gnu/libexec/kscreenlocker_greet \
         /usr/libexec/kscreenlocker_greet /usr/lib/libexec/kscreenlocker_greet; do
    if [ -x "$g" ]; then
        greeter=$g
        break
    fi
done
if [ -z "$greeter" ]; then
    echo "run-greeter: kscreenlocker_greet not found" >&2
    exit 1
fi
if [ ! -d "$stock" ]; then
    echo "run-greeter: no stock lock screen folder at $stock" >&2
    exit 1
fi

tmp=$(mktemp -d "${TMPDIR:-/tmp}/raye-locktest.XXXXXX")
trap 'rm -rf -- "$tmp"' EXIT INT TERM

shell="$tmp/share/plasma/shells/org.raye.locktest"
mkdir -p "$shell/contents/lockscreen" "$tmp/qml/org/raye"
cp "$here/testshell/metadata.json" "$shell/"
# plain copies, no links: the greeter's package loader must see real files
cp -R "$stock/." "$shell/contents/lockscreen/"
for f in "$lock"/*.qml "$lock"/*.js; do
    cp "$f" "$shell/contents/lockscreen/"
done
if [ -d "$lock/badges" ]; then
    cp -R "$lock/badges" "$shell/contents/lockscreen/"
fi

cp -R "$repo/signin/org/raye/signin" "$tmp/qml/org/raye/"
pal="$tmp/qml/org/raye/signin/Palette.qml"
sed -e "s|file:///usr/share/raye/signin/|file://$repo/assets/signin/|" \
    -e "s|file:///usr/share/wallpapers/Raye/contents/images/|file://$repo/wallpaper/Raye/contents/images/|" \
    -e "s|file:///usr/share/pixmaps/raye-eye.png|file://$repo/assets/logo/raye-eye.png|" \
    "$pal" > "$pal.new"
mv "$pal.new" "$pal"

XDG_DATA_DIRS="$tmp/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}" \
QML_IMPORT_PATH="$tmp/qml" \
    "$greeter" --testing --shell org.raye.locktest "$@"
