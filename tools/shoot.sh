#!/bin/sh
# Captures every page of the Raye gallery (tools/gallery) under Xvfb, in
# each colour scheme, scale and screen size asked for, as PNG files:
#   <page>-<scheme>-<scale in %>-<width>x<height>.png
# These are the pictures the Raye style and QML pieces are compared with
# the WinUI 3 Gallery by. They are never committed.
#
#   tools/shoot.sh                          every page, night and day, 100/125/150 %
#   tools/shoot.sh --pages w-menus,q-menus --schemes day --scales 1.5
#
# Runs with an empty configuration folder, so a user's own settings never
# change the pictures.
set -euf

here=$(cd "$(dirname "$0")" && pwd)
repo=$(dirname "$here")

usage() {
    cat <<USAGE
Usage: tools/shoot.sh [options]
  --gallery FILE     raye-gallery to run (default: build it in the cache folder)
  --out DIR          where the pictures go (default: \${TMPDIR:-/tmp}/raye-gallery-shots)
  --display :N       Xvfb display to start (default :99, must be free)
  --pages LIST       comma-separated page ids, or all (default all)
  --schemes LIST     night,day (default night,day)
  --scales LIST      for example 1,1.25,1.5 (default 1,1.25,1.5)
  --sizes LIST       for example 1280x800,1920x1080 (default 1280x800)
  --style NAME       widget style (default: what the gallery picks)
USAGE
}

die() {
    echo "shoot.sh: $*" >&2
    exit 2
}

gallery=""
out="${TMPDIR:-/tmp}/raye-gallery-shots"
display=":99"
pages="all"
schemes="night,day"
scales="1,1.25,1.5"
sizes="1280x800"
style=""

while [ $# -gt 0 ]; do
    case "$1" in
        -h|--help) usage; exit 0 ;;
        --gallery|--out|--display|--pages|--schemes|--scales|--sizes|--style)
            [ $# -ge 2 ] || die "$1 needs a value"
            case "$1" in
                --gallery) gallery=$2 ;;
                --out) out=$2 ;;
                --display) display=$2 ;;
                --pages) pages=$2 ;;
                --schemes) schemes=$2 ;;
                --scales) scales=$2 ;;
                --sizes) sizes=$2 ;;
                --style) style=$2 ;;
            esac
            shift 2 ;;
        *) usage >&2; die "unknown option: $1" ;;
    esac
done

# every value goes to a command line or a file name: check its shape first
matches() {
    printf '%s\n' "$1" | grep -Eqx "$2"
}
check_list() {
    # $1 option name, $2 list, $3 pattern of one item
    [ -n "$2" ] || die "$1 is empty"
    for item in $(printf '%s' "$2" | tr ',' ' '); do
        matches "$item" "$3" || die "$1: bad value '$item'"
    done
    matches "$2" "[^, ]+(,[^, ]+)*" || die "$1: bad list '$2'"
}
matches "$display" ":[0-9]{1,4}" || die "--display is :N, not '$display'"
[ "$pages" = all ] || check_list --pages "$pages" "[a-z]-[a-z]+"
check_list --schemes "$schemes" "night|day"
check_list --scales "$scales" "[0-9](\.[0-9]{1,2})?"
check_list --sizes "$sizes" "[0-9]{3,4}x[0-9]{3,4}"
[ -z "$style" ] || matches "$style" "[A-Za-z0-9_-]+" || die "--style: bad name '$style'"

command -v Xvfb >/dev/null 2>&1 || die "Xvfb is not installed"

if [ -z "$gallery" ]; then
    build="${XDG_CACHE_HOME:-${HOME:-/tmp}/.cache}/raye-gallery-build"
    generator=""
    command -v ninja >/dev/null 2>&1 && generator="-G Ninja"
    # shellcheck disable=SC2086
    cmake -S "$here/gallery" -B "$build" $generator -DBUILD_TESTING=OFF >/dev/null
    cmake --build "$build" -j"$(nproc 2>/dev/null || echo 2)" >/dev/null
    gallery="$build/raye-gallery"
fi
[ -x "$gallery" ] || die "no gallery program at $gallery"

if [ "$pages" = all ]; then
    pages=$("$gallery" --list-pages | cut -f1 | tr '\n' ',' | sed 's/,$//')
fi

mkdir -p "$out"
work=$(mktemp -d)
xvfb_pid=""
cleanup() {
    if [ -n "$xvfb_pid" ]; then
        kill "$xvfb_pid" 2>/dev/null || true
        wait "$xvfb_pid" 2>/dev/null || true
    fi
    rm -rf "$work"
}
trap cleanup EXIT
trap 'exit 130' INT TERM

number=${display#:}
socket="/tmp/.X11-unix/X$number"

start_xvfb() {
    [ ! -e "$socket" ] || die "display $display is in use (pick another with --display)"
    Xvfb "$display" -screen 0 "${1}x24" -nolisten tcp >"$work/xvfb.log" 2>&1 &
    xvfb_pid=$!
    tries=0
    while [ ! -e "$socket" ]; do
        tries=$((tries + 1))
        [ $tries -le 100 ] || { cat "$work/xvfb.log" >&2; die "Xvfb did not start on $display"; }
        kill -0 "$xvfb_pid" 2>/dev/null || { cat "$work/xvfb.log" >&2; die "Xvfb stopped on $display"; }
        sleep 0.1
    done
}

stop_xvfb() {
    kill "$xvfb_pid" 2>/dev/null || true
    wait "$xvfb_pid" 2>/dev/null || true
    xvfb_pid=""
    # Xvfb removes its socket when it ends; wait for it so the next start works
    tries=0
    while [ -e "$socket" ] && [ $tries -lt 50 ]; do
        tries=$((tries + 1))
        sleep 0.1
    done
}

mkdir -p "$work/config" "$work/runtime"
chmod 700 "$work/runtime"

failed=0
taken=0
for size in $(printf '%s' "$sizes" | tr ',' ' '); do
    start_xvfb "$size"
    for scheme in $(printf '%s' "$schemes" | tr ',' ' '); do
        for scale in $(printf '%s' "$scales" | tr ',' ' '); do
            percent=$(awk -v s="$scale" 'BEGIN { printf "%d", s * 100 + 0.5 }')
            for page in $(printf '%s' "$pages" | tr ',' ' '); do
                file="$out/$page-$scheme-$percent-$size.png"
                set -- --page "$page" --scheme "$scheme" --scale "$scale" --shoot "$file"
                # this repo's colours and QML pieces, wherever the program was built
                [ ! -d "$repo/colors" ] || set -- "$@" --colors-dir "$repo/colors"
                [ ! -d "$repo/qml/org/raye/ui" ] || set -- "$@" --qml-import "$repo/qml"
                [ -z "$style" ] || set -- "$@" --style "$style"
                code=0
                env DISPLAY="$display" QT_QPA_PLATFORM=xcb QT_QPA_PLATFORMTHEME= \
                    QT_QUICK_BACKEND="${QT_QUICK_BACKEND:-software}" \
                    XDG_CONFIG_HOME="$work/config" XDG_RUNTIME_DIR="$work/runtime" \
                    "$gallery" "$@" 2>"$work/err" || code=$?
                case $code in
                    0) taken=$((taken + 1)); echo "$file" ;;
                    3) taken=$((taken + 1)); failed=$((failed + 1))
                       echo "$file (page problems:)" >&2; cat "$work/err" >&2 ;;
                    *) failed=$((failed + 1))
                       echo "failed: $page $scheme $scale $size (exit $code)" >&2; cat "$work/err" >&2 ;;
                esac
            done
        done
    done
    stop_xvfb
done

echo "$taken pictures in $out, $failed problems"
[ "$failed" -eq 0 ]
