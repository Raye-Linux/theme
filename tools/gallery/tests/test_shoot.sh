#!/bin/sh
# Runs tools/shoot.sh on two pages and checks the pictures are there and
# not blank. Skipped (77) without Xvfb.
set -eu
shoot=$1
gallery=$2
command -v Xvfb >/dev/null 2>&1 || { echo "no Xvfb: skipped"; exit 77; }

out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT INT TERM
display=${RAYE_GALLERY_TEST_DISPLAY:-:197}

sh "$shoot" --gallery "$gallery" --out "$out" --display "$display" \
    --pages w-buttons,q-menus --schemes night,day --scales 1 --sizes 1280x800

count=0
for f in "$out"/w-buttons-night-100-1280x800.png "$out"/w-buttons-day-100-1280x800.png \
         "$out"/q-menus-night-100-1280x800.png "$out"/q-menus-day-100-1280x800.png; do
    [ -s "$f" ] || { echo "missing $f"; exit 1; }
    count=$((count + 1))
done
# a blank capture has a single colour
if command -v identify >/dev/null 2>&1; then
    colours=$(identify -format '%k' "$out/w-buttons-night-100-1280x800.png")
    [ "$colours" -gt 50 ] || { echo "blank capture ($colours colours)"; exit 1; }
fi
# shoot.sh refuses a bad page name instead of passing it on
if sh "$shoot" --gallery "$gallery" --out "$out" --display "$display" --pages 'w-buttons;rm' 2>/dev/null; then
    echo "bad page name accepted"; exit 1
fi
echo "ok: $count pictures"
