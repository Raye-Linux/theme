# Sourced by the Plasma session at login, before the desktop starts
# (installed to /etc/xdg/plasma-workspace/env/). Sourced, not run: it
# sets no variable and prints nothing, as the session reads its output.
#
# GTK apps: Plasma's GTK bridge (the gtkconfig module of kded) writes the
# Raye colours into ~/.config/gtk-3.0 and gtk-4.0 and draws Raye's title
# bar buttons for GTK windows each time the session starts. It only
# writes into folders that already exist, and only draws the buttons
# once the account's own settings.ini names the Breeze GTK theme. On a
# new account neither is true at the first login: GTK 4 apps kept stock
# colours and GTK title bars (Firefox's) stock round buttons, until the
# colour scheme happened to change. Make both true before kded starts.
# An account that picked another GTK theme keeps it.
#
# Flatpak apps: they read fonts only from the account's own fontconfig
# folder, never from /etc/fonts, so Raye's font rules (Inter for
# sans-serif and the Windows font names, Cascadia Mono for monospace)
# are copied there; the Flatpak override "xdg-config/fontconfig:ro"
# (/usr/share/raye-store/flatpak/overrides) lets them read it.
raye_apps_look() {
    cfg=${XDG_CONFIG_HOME:-$HOME/.config}
    for v in 3.0 4.0; do
        dir=$cfg/gtk-$v
        ini=$dir/settings.ini
        mkdir -p "$dir" || continue
        if [ ! -e "$ini" ]; then
            printf '[Settings]\ngtk-theme-name=Breeze\n' > "$ini"
        elif ! grep -q '^gtk-theme-name=' "$ini"; then
            if grep -q '^\[Settings\]' "$ini"; then
                sed -i '/^\[Settings\]/a gtk-theme-name=Breeze' "$ini"
            else
                sed -i '1i [Settings]\ngtk-theme-name=Breeze' "$ini"
            fi
        fi
    done

    fonts=${RAYE_FONTS_CONF:-/etc/fonts/conf.d/50-raye-fonts.conf}
    mine=$cfg/fontconfig/conf.d/50-raye-fonts.conf
    if [ -r "$fonts" ] && ! cmp -s "$fonts" "$mine"; then
        mkdir -p "${mine%/*}" && cp "$fonts" "$mine.new" && mv "$mine.new" "$mine"
    fi
}
# "|| :" is not decoration. This file is SOURCED by the session, so a
# non-zero status here is the sourcing shell's status, and a shell running
# with "set -e" stops at it -- every env script after this one would then
# never run, because a folder in someone's home was read-only. Measured:
# with ~/.config at mode 0500 the function returns 1 and `sh -e -c
# '. raye-apps-look.sh; echo ok'` prints nothing at all.
# This script has nothing to report and no way to report it (its output is
# discarded on purpose: the session parses what env scripts print), so the
# only honest thing it can do with a failure is not propagate it.
raye_apps_look >/dev/null 2>&1 || :
unset -f raye_apps_look
