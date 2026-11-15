#!/bin/sh
# Checks session/raye-tray-defaults:
#
#  - first login: the tray is tidied, and the phone item (KDE Connect)
#    is taken out when no phone is paired;
#  - later logins do nothing (no D-Bus call) until a phone is paired,
#    then the item is put back once;
#  - an account tidied by an older version gets the phone item out once;
#  - the script given to plasmashell, run with a stand-in for Plasma's
#    scripting (qml6, no display needed), edits the tray lists as said and
#    never writes an empty list.
#
# Needs qml6.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
script=$here/../raye-tray-defaults

work=$(mktemp -d "${TMPDIR:-/tmp}/raye-tray.XXXXXX")
trap 'rm -r -f -- "$work"' EXIT INT TERM
failures=0
ok() { echo "ok   $1"; }
bad() { echo "FAIL $1"; failures=$((failures + 1)); }

# gdbus stand-in: keeps the script it was given, answers like plasmashell
mkdir -p "$work/bin"
cat > "$work/bin/gdbus" <<'EOF'
#!/bin/sh
for last; do :; done
printf '%s' "$last" > "$GDBUS_LOG"
EOF
chmod +x "$work/bin/gdbus"

run() {
    rm -f "$work/sent.js"
    HOME=$work/home XDG_STATE_HOME= XDG_CONFIG_HOME= GDBUS_LOG=$work/sent.js \
        PATH=$work/bin:$PATH sh "$script"
}
state=$work/home/.local/state/raye

# stand-in for plasmashell: two panels, two trays; the second argument is
# each tray's extraItems (| between trays), "" for a tray that never wrote it
qml_check() { # name, extra lists, script file, expected lists after (- for unwritten), tidy|keep
    cat > "$work/t.qml" <<EOF
import QtQuick
QtObject {
    Component.onCompleted: {
        var store = {};
        function box(id, extra) {
            var cfg = {"General/extraItems": extra};
            return {
                id: id, currentConfigGroup: [],
                readConfig: function (k) { var v = cfg[this.currentConfigGroup.join("/") + "/" + k]; return v === undefined ? "" : v; },
                writeConfig: function (k, v) { cfg[this.currentConfigGroup.join("/") + "/" + k] = v; store[id + ":" + k] = v; },
                reloadConfig: function () {}
            };
        }
        var lists = "$2".split("|");
        // the first tray keeps its settings itself (Plasma 6.6), the second
        // one on a containment of its own (older Plasma)
        var own = box(10, lists[0]);
        var inner = box(20, lists[1]);
        own.readConfig = (function (read) {
            return function (k) { return this.currentConfigGroup.length == 0 && k == "SystrayContainmentId" ? "" : read.call(this, k); };
        })(own.readConfig);
        var old = { currentConfigGroup: [], readConfig: function (k) { return k == "SystrayContainmentId" ? 20 : ""; } };
        function panels() {
            return [{ widgets: function (n) { return n == "org.kde.plasma.systemtray" ? [own] : []; } },
                    { widgets: function (n) { return n == "org.kde.plasma.systemtray" ? [old] : []; } }];
        }
        function desktopById(id) { return id == 20 ? inner : null; }
        // the script runs in a scope of its own, as in plasmashell
        try {
            (function () {
$(cat "$3")
            })();
        } catch (e) {
            console.log("FAIL $1 script threw: " + e);
        }
        var want = "$4".split("|");
        var got = [store["10:extraItems"], store["20:extraItems"]];
        for (var n = 0; n < 2; ++n) {
            var w = want[n] == "-" ? undefined : want[n];
            console.log((got[n] === w ? "ok   " : "FAIL ") + "$1 tray " + n + ": " + got[n]);
        }
        var hidden = store["10:hiddenItems"] !== undefined && store["20:hiddenItems"] !== undefined
            && store["10:hiddenItems"].indexOf("org.kde.plasma.clipboard") >= 0;
        console.log((hidden == ("$5" == "tidy") ? "ok   " : "FAIL ") + "$1 hidden items written: " + hidden);
        Qt.exit(0);
    }
}
EOF
    if QT_QPA_PLATFORM=offscreen timeout 60 qml6 "$work/t.qml" > "$work/qml.out" 2>&1; then
        sed -n 's/^.*\(\(ok  \|FAIL\) \)/\1/p' "$work/qml.out" > "$work/qml.res"
        cat "$work/qml.res"
        failures=$((failures + $(grep -c '^FAIL' "$work/qml.res" || true)))
        [ "$(grep -c '^ok ' "$work/qml.res" || true)" -eq 3 ] || { bad "$1: qml check did not run"; cat "$work/qml.out"; }
    else
        bad "$1: qml6 failed"; cat "$work/qml.out"
    fi
}

full="org.kde.kdeconnect,org.kde.plasma.battery,org.kde.plasma.volume"

# 1. first login, no phone
run
[ -s "$work/sent.js" ] && ok "first login talks to plasmashell" || bad "first login talks to plasmashell"
grep -q 'var tidy = true;' "$work/sent.js" && ok "first login tidies" || bad "first login tidies"
grep -q 'var phone = "hidden";' "$work/sent.js" && ok "no phone: item hidden" || bad "no phone: item hidden"
[ -e "$state/tray-defaults-done" ] && ok "done mark written" || bad "done mark written"
[ "$(cat "$state/tray-phone")" = hidden ] && ok "phone state kept" || bad "phone state kept"
qml_check "hide" "$full|" "$work/sent.js" "org.kde.plasma.battery,org.kde.plasma.volume|-" tidy

# the phone item alone in the list: never written as an empty list
qml_check "alone" "org.kde.kdeconnect|" "$work/sent.js" "-|-" tidy

# 2. next login, nothing changed
run
[ ! -e "$work/sent.js" ] && ok "later login: no call" || bad "later login: no call"

# 3. a phone got paired
mkdir -p "$work/home/.config/kdeconnect"
printf '[abc123]\nname=Phone\n' > "$work/home/.config/kdeconnect/trusted_devices"
run
grep -q 'var tidy = false;' "$work/sent.js" && ok "paired: tray not tidied again" || bad "paired: tray not tidied again"
grep -q 'var phone = "shown";' "$work/sent.js" && ok "paired: item shown" || bad "paired: item shown"
[ "$(cat "$state/tray-phone")" = shown ] && ok "paired state kept" || bad "paired state kept"
qml_check "show" "org.kde.plasma.battery|$full" "$work/sent.js" "org.kde.plasma.battery,org.kde.kdeconnect|-" keep
run
[ ! -e "$work/sent.js" ] && ok "paired, later login: no call" || bad "paired, later login: no call"

# 4. an account tidied by an older version, no phone
rm -r "$work/home"
mkdir -p "$state"
touch "$state/tray-defaults-done"
run
grep -q 'var tidy = false;' "$work/sent.js" && grep -q 'var phone = "hidden";' "$work/sent.js" \
    && ok "older account: item taken out once" || bad "older account: item taken out once"
run
[ ! -e "$work/sent.js" ] && ok "older account, later login: no call" || bad "older account, later login: no call"

echo
if [ "$failures" -gt 0 ]; then
    echo "$failures check(s) failed"
    exit 1
fi
echo "all checks passed"
