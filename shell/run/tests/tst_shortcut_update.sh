#!/bin/sh
# raye-run-shortcut.sh: an empty Win+R entry goes, a chosen one stays,
# other shortcuts are not touched.
#   tst_shortcut_update.sh <path to raye-run-shortcut.sh>
set -eu
script=$1
command -v kreadconfig6 >/dev/null && command -v kwriteconfig6 >/dev/null || exit 77
dir=$(mktemp -d)
trap 'rm -rf "$dir"' EXIT
export XDG_CONFIG_HOME="$dir"
file="$dir/kglobalshortcutsrc"
fail() { echo "FAIL: $1"; cat "$file"; exit 1; }

printf '[services][org.kde.spectacle.desktop]\nRecordRegion=Meta+Shift+R\n\n[services][org.raye.run.desktop]\n_launch=\n' > "$file"
sh "$script"
grep -q '_launch' "$file" && fail "the empty entry is still there"
grep -q 'RecordRegion=Meta+Shift+R' "$file" || fail "another shortcut was touched"

printf '[services][org.raye.run.desktop]\n_launch=Meta+F12\n' > "$file"
sh "$script"
grep -q '_launch=Meta+F12' "$file" || fail "the user's own shortcut was removed"

: > "$file"
sh "$script"
grep -q 'raye.run' "$file" && fail "an entry was added"
echo PASS
