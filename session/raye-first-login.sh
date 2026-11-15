# Sourced by the Plasma session at login, before the splash starts
# (installed to /etc/xdg/plasma-workspace/env/). Sourced, not run: it sets
# no variable and prints nothing, as the session reads its output.
#
# The very first sign-in of an account shows "Hi" instead of "Welcome".
# ksplash gives its QML no way to ask, so the question is answered with a
# file: a marker picture the splash tries to load. It is there for one
# login, and first-login-done keeps it from coming back.
#
# An account that already has a desktop is not new: it got here by an
# upgrade, and greeting it again would be wrong. The live session is a
# guest, not a new account.
raye_first_login() {
    state=${XDG_STATE_HOME:-$HOME/.local/state}/raye
    done_file=$state/first-login-done
    marker=$state/first-login.png
    if [ -e "$done_file" ]; then
        rm -f -- "$marker" 2>/dev/null
        return 0
    fi
    mkdir -p -- "$state" 2>/dev/null || return 0
    case " $(cat "${RAYE_CMDLINE:-/proc/cmdline}" 2>/dev/null) " in
        *" boot=casper "*) : > "$done_file" 2>/dev/null; return 0 ;;
    esac
    if [ ! -e "${XDG_CONFIG_HOME:-$HOME/.config}/plasma-org.kde.plasma.desktop-appletsrc" ]; then
        cp -- "${RAYE_SIGNIN_DIR:-/usr/share/raye/signin}/first-login-marker.png" "$marker" 2>/dev/null
    fi
    : > "$done_file" 2>/dev/null
    return 0
}
raye_first_login
unset -f raye_first_login
unset state done_file marker
