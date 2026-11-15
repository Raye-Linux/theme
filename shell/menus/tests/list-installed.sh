#!/bin/sh
# Lists every installed app and the Start category it lands in, with the
# Raye menu, on a running system. Uses a cache of its own, so the cache
# of the session is never touched.
#   list-installed.sh [path to raye-menu-categories]
set -eu
tool=${1:-raye-menu-categories}
dir=$(mktemp -d)
trap 'rm -rf "$dir"' EXIT INT TERM
XDG_MENU_PREFIX=raye- KDESYCOCA="$dir/ksycoca" "$tool"
