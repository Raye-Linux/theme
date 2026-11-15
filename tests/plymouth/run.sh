#!/bin/sh
# Draws the Raye boot screen with plymouthd and its X11 renderer under
# Xvfb, in every state, and saves a screenshot of each.
#
#   sudo tests/plymouth/run.sh [out dir] [WIDTHxHEIGHT|dual ...]
#
# "dual" draws on two screens of different sizes at once, the way a
# laptop with a monitor plugged in starts: the X11 renderer's fake
# multi-head setup, 800x600 and 640x480 side by side. check_block.py
# then checks that eye, ring and words sit as one block in the middle of
# every screen.
#
# Needs plymouth, plymouth-x11, plymouth-label, Xvfb, xdotool,
# imagemagick and fonts-inter. Runs as root in its own mount namespace:
# the theme, /run and plymouthd.conf it mounts are seen by nobody else.
# In a chroot (where / is no mount point) start it from outside instead:
#   unshare -m --propagation private chroot <dir> .../run.sh
# It refuses to mount anything while a mount could leak out.
# Display :${RAYE_DISPLAY:-120}.
set -eu

here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../.." && pwd)
out=${1:-/tmp/raye-plymouth-shots}
[ $# -gt 0 ] && shift
sizes=${*:-1280x800 1920x1080 dual}

[ "$(id -u)" = 0 ] || { echo "run as root" >&2; exit 1; }
mkdir -p "$out"
if grep -q ' shared:' /proc/self/mountinfo; then
    if [ "${RAYE_PLYMOUTH_NS:-}" != 1 ] && unshare -m --propagation private true 2>/dev/null; then
        # shellcheck disable=SC2086
        exec env RAYE_PLYMOUTH_NS=1 unshare -m --propagation private sh "$0" "$out" $sizes
    fi
    echo "mounts here are shared: start me in a private mount namespace" >&2
    exit 1
fi

work=$(mktemp -d /tmp/raye-plymouth.XXXXXX)
xvfb=""
ptyholder=""
plypid=""
cleanup() {
    # by pid: a pattern would also hit a plymouthd someone else runs here
    # shellcheck disable=SC2086
    kill $plypid $xvfb $ptyholder 2>/dev/null || true
    rm -r -f -- "$work"
}
trap cleanup EXIT
export DISPLAY=":${RAYE_DISPLAY:-120}"

# themes: the stock ones and ours, rendered for one language
themes="$work/themes"
mkdir -p "$themes" "$work/root/usr/share/plymouth/themes"
cp -a /usr/share/plymouth/themes/. "$themes/"
ln -s "$repo/plymouth/raye" "$work/root/usr/share/plymouth/themes/raye"
mount --bind "$themes" /usr/share/plymouth/themes
mount -t tmpfs tmpfs /run
printf '[Daemon]\nTheme=raye\nShowDelay=0\nDeviceTimeout=1\n' > "$work/plymouthd.conf"
mount --bind "$work/plymouthd.conf" /etc/plymouth/plymouthd.conf
# the initramfs draws text in grey antialiasing (see the hook)
cat > "$work/fonts.conf" <<'EOF'
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">
<fontconfig>
  <include>/etc/fonts/fonts.conf</include>
  <match target="font">
    <edit name="rgba" mode="assign"><const>none</const></edit>
  </match>
</fontconfig>
EOF
export FONTCONFIG_FILE="$work/fonts.conf"

theme() { # lang keyboard tpm
    rm -rf "$themes/raye"
    cp -a "$repo/plymouth/raye" "$themes/raye"
    RAYE_PLYMOUTH_ROOT="$work/root" sh "$repo/initramfs/raye-plymouth" render "$1" "$2" \
        > "$themes/raye/raye.script"
    rm -f "$themes/raye/raye.script.in" "$themes/raye"/strings-*
    if [ "$3" = tpm ]; then cp "$themes/raye/tpm-marker.png" "$themes/raye/tpm.png"; fi
}

# plymouthd wants a real terminal: give it a pty nobody reads
python3 -c 'import os, time
m, s = os.openpty()
print(os.ttyname(s), flush=True)
time.sleep(1e6)' > "$work/pty" &
ptyholder=$!
sleep 0.5
tty=$(cat "$work/pty")

start() { # mode
    mkdir -p /run/plymouth
    dbg=""
    [ -z "${RAYE_PLYMOUTH_DEBUG:-}" ] || dbg="--debug --debug-file=$out/plymouthd-$1.log"
    # shellcheck disable=SC2086
    plymouthd --no-daemon --tty="$tty" --ignore-serial-consoles --no-boot-log --mode="$1" $dbg \
        --kernel-command-line="splash quiet" --pid-file=/run/plymouth/pid &
    plypid=$!
    i=0
    while ! plymouth --ping 2>/dev/null; do
        i=$((i + 1)); [ $i -lt 50 ] || { echo "plymouthd did not start" >&2; exit 1; }
        sleep 0.1
    done
    plymouth show-splash
    sleep 1.5
}

stop() {
    plymouth quit 2>/dev/null || true
    i=0
    while plymouth --ping 2>/dev/null && [ $i -lt 30 ]; do sleep 0.1; i=$((i + 1)); done
    [ -z "$plypid" ] || kill "$plypid" 2>/dev/null || true
    [ -z "$plypid" ] || wait "$plypid" 2>/dev/null || true
    plypid=""
    sleep 0.3
}

shot() { # name
    import -window root "$out/$1.png"
    echo "  $1.png"
}

type_keys() { # text
    win=$(xdotool search --onlyvisible --name . 2>/dev/null | head -n 1 || true)
    [ -n "$win" ] && xdotool windowfocus "$win" 2>/dev/null || true
    xdotool type --delay 40 "$1"
}

# the screens of each size, as check_block.py wants them
areas() { # size
    case $1 in
    dual) echo "0,0,800,600 800,0,640,480" ;;
    *) echo "" ;;
    esac
}

for size in $sizes; do
    case $size in
    dual)
        screen=1440x600
        export PLY_CREATE_FAKE_MULTI_HEAD_SETUP=1
        ;;
    *)
        screen=$size
        unset PLY_CREATE_FAKE_MULTI_HEAD_SETUP
        ;;
    esac
    Xvfb "$DISPLAY" -screen 0 "${screen}x24" -nolisten tcp >/dev/null 2>&1 &
    xvfb=$!
    sleep 1.5
    for lang in en fr; do
        case $lang in
        en) kb="French (Canada)" ;;
        fr) kb="Français (Canada)" ;;
        esac
        p="$size-$lang"
        echo "$p"

        theme "$lang" "$kb" none

        # boot: eye and ring, nothing else; a stray message stays silent
        start boot
        plymouth display-message --text="cryptsetup: sda3_crypt: set up successfully"
        plymouth message --text="Begin: Loading essential drivers"
        sleep 0.5
        shot "$p-01-boot"

        # drive check message, then cleared
        plymouth display-message --text="raye:fsck:EFI"
        sleep 0.5
        shot "$p-02-fsck"
        plymouth display-message --text="raye:clear"

        # disk password: field, typed bullets, then a wrong one
        timeout 30 plymouth ask-for-password --prompt="Please unlock disk sda3_crypt:" > "$work/answer" &
        ask=$!
        sleep 1
        shot "$p-03-password"
        type_keys "hunter2"
        sleep 0.6
        shot "$p-04-typed"
        xdotool key Return
        wait $ask || true
        sleep 0.5
        timeout 30 plymouth ask-for-password --prompt="Please unlock disk sda3_crypt:" > "$work/answer" &
        ask=$!
        sleep 0.12
        shot "$p-05-wrong-shake"
        sleep 0.8
        shot "$p-06-wrong"
        type_keys "x"
        sleep 0.4
        shot "$p-07-wrong-cleared"
        xdotool key Return
        wait $ask || true
        stop

        # TPM bound: the ring turns while the chip works, however long it
        # takes; the initramfs says when it said no: "security settings
        # changed" and the field
        theme "$lang" "$kb" tpm
        start boot
        timeout 30 plymouth ask-for-password --prompt="Please unlock disk sda3_crypt:" > "$work/answer" &
        ask=$!
        sleep 1.5
        shot "$p-08-tpm-wait"
        sleep 5
        shot "$p-08b-tpm-slow"
        plymouth display-message --text=raye:tpm-failed
        sleep 0.5
        shot "$p-09-tpm-changed"
        xdotool key Return
        wait $ask || true
        stop

        # TPM bound, but a key is pressed: the field shows at once
        start boot
        timeout 30 plymouth ask-for-password --prompt="Please unlock disk sda3_crypt:" > "$work/answer" &
        ask=$!
        sleep 0.8
        type_keys "a"
        sleep 0.5
        shot "$p-09b-tpm-key"
        xdotool key Return
        wait $ask || true
        stop

        # TPM bound and it opens the disk: cryptsetup's question goes away
        # without a word, the initramfs ends. No field, ever, even past the
        # 20 s the screen waits for a slow chip
        start boot
        timeout 30 plymouth ask-for-password --prompt="Please unlock disk sda3_crypt:" > "$work/answer" &
        ask=$!
        sleep 1
        kill $ask 2>/dev/null || true
        wait $ask 2>/dev/null || true
        plymouth display-message --text=raye:disk-open
        sleep "${RAYE_PLYMOUTH_PAST_WAIT:-22}"
        shot "$p-09c-tpm-opened"
        stop

        theme "$lang" "$kb" none
        start shutdown
        shot "$p-10-shutdown"
        plymouth display-message --text="raye:remove-usb"
        sleep 0.4
        shot "$p-11-remove-usb"
        stop

        start reboot
        shot "$p-12-reboot"
        stop

        # what raye-update sends at start-up. By then the disk is up and
        # /usr holds the English script: the mode goes as a message, since
        # plymouth change-mode would read that one again
        start boot
        cp "$repo/plymouth/raye/raye.script" "$themes/raye/raye.script"
        plymouth display-message --text="raye:mode:updates"
        plymouth system-update --progress=0
        sleep 0.5
        plymouth system-update --progress=37
        sleep 0.5
        shot "$p-13-updates"
        plymouth display-message --text="raye:finishing-updates"
        sleep 0.4
        shot "$p-13b-finishing"
        plymouth system-update --progress=42
        sleep 0.4
        shot "$p-13c-finishing-then-42"
        # installed, and the start-up goes on to the sign-in screen
        plymouth display-message --text="raye:mode:boot"
        sleep 0.4
        shot "$p-13d-go-on"
        # the sign-in screen takes over: the eye stays, the ring goes
        plymouth display-message --text="raye:handoff"
        sleep 0.4
        shot "$p-13e-handoff"
        stop

        # firmware after the packages, as raye-update sends it; a number
        # sent here must not show, fwupd gives none while it writes
        theme "$lang" "$kb" none
        start boot
        plymouth display-message --text="raye:mode:firmware-upgrade"
        plymouth system-update --progress=37
        sleep 0.5
        shot "$p-14-firmware"
        plymouth display-message --text="raye:firmware-step:2:3"
        sleep 0.4
        shot "$p-14b-firmware-step"
        stop

        # what an offline upgrade sends once the disk is up
        start boot
        plymouth display-message --text="raye:mode:system-upgrade"
        plymouth system-update --progress=37
        sleep 0.5
        shot "$p-15-upgrade"
        stop
    done

    # Shutdown as on the installed system: no theme in plymouthd.conf,
    # the English script under /usr, and what the initramfs left in /run
    # for a French system. The screen has to speak French.
    echo "$size-run-copy"
    dest="$work/initramfs"
    rm -r -f -- "$dest"
    mkdir -p "$dest/usr/share/plymouth/themes" "$work/root/etc/default"
    cp -a "$repo/plymouth/raye" "$dest/usr/share/plymouth/themes/raye"
    printf 'LANG=fr_CA.UTF-8\n' > "$work/root/etc/default/locale"
    DESTDIR="$dest" RAYE_PLYMOUTH_ROOT="$work/root" sh "$repo/initramfs/raye-plymouth"
    rm -f "$work/root/etc/default/locale"
    sed "s|/usr/share/plymouth/themes/raye|$dest/usr/share/plymouth/themes/raye|" \
        "$dest/scripts/init-bottom/raye-plymouth" | sh
    rm -r -f -- "$themes/raye"
    cp -a "$repo/plymouth/raye" "$themes/raye"
    printf '[Daemon]\nShowDelay=0\n' > "$work/plymouthd.conf"
    start shutdown
    shot "$size-16-shutdown-from-run"
    stop
    rm -r -f -- /run/plymouth/themes /run/plymouth/plymouthd.defaults
    printf '[Daemon]\nTheme=raye\nShowDelay=0\nDeviceTimeout=1\n' > "$work/plymouthd.conf"
    kill $xvfb 2>/dev/null || true
    wait $xvfb 2>/dev/null || true
    xvfb=""
done
echo "screenshots in $out"

# the block in the middle of every screen, in every state but the one
# caught in the middle of the wrong password shake
failed=0
for size in $sizes; do
    for f in "$out/$size"-*.png; do
        case $f in *-05-wrong-shake.png) continue ;; esac
        # shellcheck disable=SC2046
        python3 "$here/check_block.py" "$f" $(areas "$size") || failed=1
    done
done
exit $failed
