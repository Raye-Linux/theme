#!/bin/sh
# Checks session/raye-first-login.sh, sourced the way the session sources it:
#
#  - a new account gets the marker and the done file, once;
#  - the next login takes the marker away and does not bring it back;
#  - an account that already has a desktop (an upgrade) gets no marker;
#  - the live session gets no marker;
#  - XDG_STATE_HOME is honoured;
#  - it prints nothing, leaves no variable behind and never fails, even
#    with a home it cannot write to.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
script=$here/../raye-first-login.sh
assets=$here/../../assets/signin

work=$(mktemp -d "${TMPDIR:-/tmp}/raye-first.XXXXXX")
trap 'chmod -R u+rwx "$work"; rm -r -f -- "$work"' EXIT INT TERM

fails=0
checks=0
ok() {
    checks=$((checks + 1))
    if ! eval "$2"; then
        echo "FAIL: $1"
        fails=$((fails + 1))
    fi
}

cmdline=$work/cmdline
echo "BOOT_IMAGE=/vmlinuz root=/dev/mapper/root ro quiet" > "$cmdline"

# one login: prints what the session would read
login() {
    (
        HOME=$1
        unset XDG_CONFIG_HOME
        XDG_STATE_HOME=${2:-}
        [ -n "$XDG_STATE_HOME" ] || unset XDG_STATE_HOME
        RAYE_SIGNIN_DIR=$assets
        RAYE_CMDLINE=${3:-$cmdline}
        export HOME RAYE_SIGNIN_DIR RAYE_CMDLINE
        . "$script"
        for v in state done_file marker; do
            eval "[ -z \"\${$v+x}\" ]" || echo "LEFT:$v"
        done
        command -v raye_first_login >/dev/null && echo "LEFT:function"
        exit 0
    )
}

h=$work/new
mkdir -p "$h"
out=$(login "$h")
s=$h/.local/state/raye
ok "first login prints nothing and leaves nothing" '[ -z "$out" ]'
ok "first login makes the marker" '[ -s "$s/first-login.png" ]'
ok "the marker is the picture we ship" 'cmp -s "$s/first-login.png" "$assets/first-login-marker.png"'
ok "first login makes the done file" '[ -e "$s/first-login-done" ]'

out=$(login "$h")
ok "second login prints nothing" '[ -z "$out" ]'
ok "second login takes the marker away" '[ ! -e "$s/first-login.png" ]'
ok "second login keeps the done file" '[ -e "$s/first-login-done" ]'
login "$h" >/dev/null
ok "third login does not bring it back" '[ ! -e "$s/first-login.png" ]'

h=$work/upgrade
mkdir -p "$h/.config"
: > "$h/.config/plasma-org.kde.plasma.desktop-appletsrc"
login "$h" >/dev/null
s=$h/.local/state/raye
ok "an account with a desktop is not greeted" '[ ! -e "$s/first-login.png" ]'
ok "and it is not asked again" '[ -e "$s/first-login-done" ]'

h=$work/live
mkdir -p "$h"
echo "BOOT_IMAGE=/casper/vmlinuz boot=casper quiet" > "$work/live-cmdline"
login "$h" "" "$work/live-cmdline" >/dev/null
s=$h/.local/state/raye
ok "the live session is not greeted" '[ ! -e "$s/first-login.png" ]'

h=$work/xdg
mkdir -p "$h"
login "$h" "$work/xdg-state" >/dev/null
ok "XDG_STATE_HOME is honoured" '[ -s "$work/xdg-state/raye/first-login.png" ]'
ok "and the home is left alone" '[ ! -e "$h/.local" ]'

# a home it cannot write to: root writes anywhere, so make the place a file
h=$work/stuck
mkdir -p "$h"
: > "$h/.local"
if out=$(login "$h" 2>&1); then rc=0; else rc=$?; fi
ok "an unwritable home does not fail the login" '[ "$rc" = 0 ]'
ok "and says nothing" '[ -z "$out" ]'

# a set -u session must not trip over it
h=$work/nounset
mkdir -p "$h"
if ( set -u; login "$h" ) >/dev/null 2>&1; then rc=0; else rc=$?; fi
ok "set -u does not trip it" '[ "$rc" = 0 ]'

echo "$checks checks, $fails failed"
[ "$fails" = 0 ]
