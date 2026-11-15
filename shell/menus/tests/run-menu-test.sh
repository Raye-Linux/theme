#!/bin/sh
# Builds a private menu cache from tests/fixtures with the Raye menu, in
# English and in French, and compares the result with expected-*.txt.
#   run-menu-test.sh <raye-menu-categories> <shell/menus folder>
set -eu
tool=$1
src=$2
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT INT TERM

mkdir -p "$work/config/menus" "$work/data/desktop-directories" "$work/data/applications"
cp "$src/raye-applications.menu" "$work/config/menus/"
cp "$src"/desktop-directories/*.directory "$work/data/desktop-directories/"
cp "$src"/tests/fixtures/applications/*.desktop "$work/data/applications/"

status=0
for lang in en fr; do
    case $lang in
        en) locale=en_US.UTF-8 ;;
        fr) locale=fr_FR.UTF-8 ;;
    esac
    home="$work/home-$lang"
    mkdir -p "$home"
    env -i PATH=/usr/bin:/bin HOME="$home" \
        XDG_CONFIG_HOME="$home/.config" XDG_DATA_HOME="$home/.local/share" \
        XDG_CACHE_HOME="$home/.cache" XDG_RUNTIME_DIR="$home" \
        XDG_CONFIG_DIRS="$work/config" XDG_DATA_DIRS="$work/data" \
        XDG_MENU_PREFIX=raye- XDG_CURRENT_DESKTOP=KDE KDESYCOCA="$work/ksycoca-$lang" \
        LANG=$locale LANGUAGE=$lang LC_ALL=$locale QT_QPA_PLATFORM=offscreen \
        "$tool" > "$work/out-$lang.txt" 2> "$work/err-$lang.txt" || {
        echo "raye-menu-categories failed ($lang):"; cat "$work/err-$lang.txt"; exit 1; }
    if ! diff -u "$src/tests/expected-$lang.txt" "$work/out-$lang.txt"; then
        echo "menu categories differ from tests/expected-$lang.txt ($lang)"
        status=1
    fi
done
exit $status
