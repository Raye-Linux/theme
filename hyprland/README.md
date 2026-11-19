# Raye on Hyprland

The `raye-hyprland` package: the Raye desktop on the Hyprland compositor,
chosen at sign-in next to the Plasma one ("Raye (Hyprland)").

| Path | What it is |
|---|---|
| `session/raye-hyprland.desktop` | the sign-in screen's entry (`/usr/share/wayland-sessions`) |
| `session/raye-hyprland-session` | the session command: writes `~/.config/raye-hyprland/` and starts Hyprland |
| `session/raye-hyprland-start` | run once Hyprland is up: the shell, hypridle, the root prompt, autostart |
| `config/hyprland.conf` | Raye's Hyprland settings: keys, window rules, motion, blur |
| `config/hyprlock.conf`, `config/hypridle.conf` | the lock screen and when it comes up |
| `shell/` | `raye-hypr-shell`, the host of the Raye taskbar, Start, quick settings and toasts |
| `tools/raye-hypr-screenshot` | Win+Shift+S |

## How the shell works

Plasma's shell cannot run on Hyprland, but the Raye views can: the
taskbar row, Start, quick settings and the toast card know nothing of
Plasma (the applets wrap them). `raye-hypr-shell` loads those files from
the installed applets (`plasma/plasmoids/org.raye.*/contents/ui`, see
`shell/src/views.h`), puts them in layer-shell windows (LayerShellQt) and
feeds them from Hyprland:

- `shell/src/hyprland.*` talks to Hyprland's request and event sockets;
- `shell/qml/TaskController.qml` turns the window list into the model and
  controller the taskbar row expects;
- `shell/qml/hyprlogic.js` holds the decisions (rows, requests, Win+arrows);
- the keys in `hyprland.conf` reach the shell over D-Bus
  (`raye-hypr-shell --toggle start`, `--snap left`...).

So a change to the taskbar or Start shows on both desktops, and
`raye-hyprland` depends on the very same build of `raye-shell`.

## User settings

`~/.config/raye-hyprland/hyprland.conf` sources Raye's file, the keyboard
of the machine, then `user.conf`: a user's own lines go in `user.conf` and
win.

## Tests

`ctest` runs `tst_hyprlogic.qml`, the socket link against a stand-in
(`tst_hyprland.cpp`) and `test_session.py`, which parses the settings with
the Hyprland of the build (`Hyprland --verify-config`).

Hyprland needs a GPU, so pictures are taken another way:
`tests/preview.sh` runs the shell on sway's headless backend with
`tests/fake-hyprland.py` standing in for Hyprland's sockets.
