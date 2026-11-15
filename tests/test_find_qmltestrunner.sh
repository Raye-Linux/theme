#!/bin/sh
# tests/find-qmltestrunner.sh: it finds the runner where each distribution
# puts it, and it SKIPS with 77 when there is none.
#   tests/test_find_qmltestrunner.sh
#
# Why this is tested at all. The four QML suites of this repository ran
# nowhere for months: no CMakeLists registered them, and the launcher's
# suite table carried a frozen string, "qmltestrunner is not installed",
# that no code ever re-evaluated. Installing qmltestrunner did not change
# it. 355 tests, all green the moment they were run.
#
# So the resolver is the piece that decides whether those 355 tests run or
# are silently declared impossible -- and a resolver that answers "not
# installed" about an installed binary is the whole defect, in one line.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
helper=$here/find-qmltestrunner.sh
fail=0
ok()  { echo "ok   $1"; }
bad() { echo "FAIL $1"; fail=1; }

work=$(mktemp -d "${TMPDIR:-/tmp}/raye-fqtr.XXXXXX")
trap 'rm -r -f -- "$work"' EXIT INT TERM
mkdir -p "$work/vide" "$work/qtbin" "$work/onpath"
mkdir -p "$work/broken"
# A candidate that answers -help, like the real thing.
printf '#!/bin/sh\n[ "$1" = -help ] && exit 0\necho stub\n' \
    > "$work/qtbin/qmltestrunner"
printf '#!/bin/sh\n[ "$1" = -help ] && exit 0\necho stub\n' \
    > "$work/onpath/qmltestrunner"
# And one that behaves exactly like Ubuntu's /usr/bin/qmltestrunner,
# which is a symlink to qtchooser: it exists, it is executable, and it
# cannot run. Measured in the resolute chroot:
#   qmltestrunner: could not exec '/usr/lib/qt5/bin/qmltestrunner'  (exit 1)
printf '#!/bin/sh\necho "qmltestrunner: could not exec qt5" >&2\nexit 1\n' \
    > "$work/broken/qmltestrunner"
# A qmake6 that answers nothing, beside the stub on PATH. Without it
# the real qmake6 of this machine points the search at the REAL
# /usr/lib/qt6/bin before PATH is ever reached, and the PATH case below
# passes or fails for a reason that has nothing to do with PATH.
printf '#!/bin/sh\nexit 1\n' > "$work/onpath/qmake6"
chmod 0755 "$work/qtbin/qmltestrunner" "$work/onpath/qmltestrunner" \
    "$work/onpath/qmake6" "$work/broken/qmltestrunner"

# Each case runs in a child shell: the helper exits 77 on the skip path, and
# an `exit` inside a sourced file would take this script down with it.
#
# The first version of this test called `env VAR=... sh -c`, and `env`
# looks the command up in the PATH it was just handed: with PATH=/nonexistent
# every case died with "env: 'sh': No such file or directory" -- exit 127,
# which looks exactly like a broken helper. /bin/sh by absolute path, and one
# variable per argument so a value with a space is not split into a command.
run() { # name, expected exit, expected QMLTESTRUNNER, then VAR=value...
    _name=$1; _code=$2; _want=$3; shift 3
    set +e
    _out=$("$@" /bin/sh -c '
        set -eu
        . "$1"
        raye_find_qmltestrunner
        printf "%s\n" "$QMLTESTRUNNER"' _ "$helper" 2>&1)
    _got=$?
    set -e
    if [ "$_got" != "$_code" ]; then
        bad "$_name: exit $_got, expected $_code"; echo "$_out"; return
    fi
    if [ -n "$_want" ] && [ "$_out" != "$_want" ]; then
        bad "$_name: chose '$_out', expected '$_want'"; return
    fi
    if [ "$_code" = 77 ]; then
        case "$_out" in
            *"no working qmltestrunner"*) ;;
            *) bad "$_name: the skip says nothing a reader can act on: $_out"; return ;;
        esac
    fi
    ok "$_name"
}

E=/usr/bin/env

# 1. nothing anywhere -> 77, and a reason, not silence.
run "no runner at all skips with 77" 77 "" \
    $E -u QMLTESTRUNNER "RAYE_QT_BIN_DIRS=$work/vide" PATH=/nonexistent

# 2. a listed directory is used even when PATH has nothing. Two directories,
#    the first empty: the list really is a list, not just its first entry.
run "found in the second listed Qt bin directory" 0 "$work/qtbin/qmltestrunner" \
    $E -u QMLTESTRUNNER "RAYE_QT_BIN_DIRS=$work/vide $work/qtbin" PATH=/nonexistent

# 3. PATH is the LAST resort, used only when no Qt directory holds a
#    working one. It is last on purpose: see trap 2 in the helper.
run "PATH is used when no Qt directory has one" 0 "$work/onpath/qmltestrunner" \
    $E -u QMLTESTRUNNER "RAYE_QT_BIN_DIRS=$work/vide" \
    "PATH=$work/onpath:/usr/bin:/bin"

# 3b. the exact Ubuntu shape: nothing in any Qt directory, and what is
#     on PATH is the qtchooser wrapper. The answer has to be a skip, not
#     a binary that cannot run.
run "a broken one on PATH is not accepted" 77 "" \
    $E -u QMLTESTRUNNER "RAYE_QT_BIN_DIRS=$work/vide" \
    "PATH=$work/broken"

# 4. an executable handed in is taken as it stands -- this is how CMake
#    passes the one find_program() already located.
run "an explicit QMLTESTRUNNER is honoured" 0 "$work/qtbin/qmltestrunner" \
    $E "QMLTESTRUNNER=$work/qtbin/qmltestrunner" "RAYE_QT_BIN_DIRS=$work/vide" \
    PATH=/nonexistent

# 5. and a handed-in path that is NOT executable must not be trusted: a
#    stale CMake cache would otherwise make every suite fail with "permission
#    denied" instead of skipping or finding a real one.
run "a non-executable QMLTESTRUNNER is ignored" 0 "$work/qtbin/qmltestrunner" \
    $E "QMLTESTRUNNER=$work/vide/nope" "RAYE_QT_BIN_DIRS=$work/qtbin" PATH=/nonexistent

# 6. the Ubuntu trap: a candidate that exists, is executable, and does
#    NOT work must be passed over, not chosen. Without this the suites
#    would have failed with "could not exec" on the very platform Raye
#    ships on, which reads as a broken test rather than a wrong binary.
run "a broken candidate in a Qt directory is passed over" 0 \
    "$work/qtbin/qmltestrunner" \
    $E -u QMLTESTRUNNER "RAYE_QT_BIN_DIRS=$work/broken $work/qtbin" \
    PATH=/nonexistent

# 7. and when the ONLY candidate is the broken one, it skips with 77
#    rather than handing back something that cannot run.
run "only a broken candidate skips with 77" 77 "" \
    $E -u QMLTESTRUNNER "RAYE_QT_BIN_DIRS=$work/broken" PATH=/nonexistent

# 8. a handed-in QMLTESTRUNNER that does not work is dropped too: a stale
#    CMake cache must not take every suite down with it.
run "a broken QMLTESTRUNNER is dropped for a working one" 0 \
    "$work/qtbin/qmltestrunner" \
    $E "QMLTESTRUNNER=$work/broken/qmltestrunner" \
    "RAYE_QT_BIN_DIRS=$work/qtbin" PATH=/nonexistent

# 9. the real list must not be empty: an empty default would turn every
#    machine into "not installed" while looking like a search.
if grep -q '^/usr/lib/qt6/bin ' "$helper" || grep -q '"/usr/lib/qt6/bin' "$helper"; then
    ok "the default list still holds Debian's /usr/lib/qt6/bin"
else
    bad "the default list no longer holds /usr/lib/qt6/bin, where Debian puts it"
fi

[ "$fail" = 0 ] && echo "PASS"
exit "$fail"
