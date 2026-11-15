# org.raye.shell

The QML module every part of the shell builds on. Colours, radii and type
come from `org.raye.ui` (theme/qml, in raye-style); this module adds what
is the shell's own.

    import org.raye.ui      // Tokens, Acrylic, Pill, FocusRing...
    import org.raye.shell   // what is below

The shell's tokens are `ShellTokens`, not `Tokens`: two modules giving the
same type name cannot be imported together.

## Types

| Type | What it is |
|---|---|
| `ShellSettings` (singleton) | every key of `rayeshellrc`, checked, followed live; setters for what the shell writes (pins, recent searches, jump lists, focus end, tile order) |
| `ShellTokens` (singleton) | taskbar and flyout sizes, the shell's colours (also `taskbar*` colours for what sits on the taskbar, which keeps its own shade in Day), durations |
| `Motion` (singleton) | Windows' curves as `QEasingCurve` values: `easing: Motion.entrance`, `.exit`, `.pointToPoint` |
| `Launcher` (singleton) | `launch(desktopId)`, `openUrl(url)`, `openSettings(page)` (`raye-settings --page <id>`), `invokeShortcut(component, name)` |
| `UpdateState` (singleton) | Raye Update's `state.json` (docs/state.md of the update repo); `updateAndRestart()`, `updateAndShutdown()`, `armFailed(reason)` |
| `RecentlyAddedModel` | apps new in the last 7 days and not opened since (roles `desktopId`, `name`, `iconName`, `added`) |
| `GlobalShortcut` | a kglobalaccel action of `plasmashell`: `name`, `text`, `sequence`, `onActivated` |
| `BlurBehind` | blur behind a window, cut to an item's rounded shape; `active` says whether to draw see-through |
| `AcrylicSurface` | the Raye acrylic of a flyout: tint, inner line, violet rim when `focused`, opaque without blur |
| `PanelButton` | taskbar button: 40 x 40 backplate, press scale 0.9, `selected`, `toolTipText`, `onTaskbar` |
| `ActiveOutput` (singleton) | the screen in use (KWin's active output, where the pointer is): `screen()`, `geometry()` |
| `AppJumpList` | an app's tasks (.desktop actions) and recent files, for the taskbar's right-click menu |
| `PanelMenu` | a taskbar menu: RayeMenu rows in a window of its own above the bar, `open(item, x)`, `addItem()`, `clear()` |
| `RayeMenu`, `RayeMenuItem`, `RayeMenuSeparator` | the shell's menus (radius 8, rows 32, keys on the right), in a window of their own |
| `RayeSlider` | quick settings slider |
| `RayeToggleTile` | quick settings tile, 96 x 48, `split` for the "›" part |
| `RayeTooltip` | 4 px corners, 500 ms delay |

## rayeshellrc

`rayeshellrc` here is installed to `/etc/xdg` and lists every key with
its default; the key table in `src/shellsettings.cpp` must say the same
(a test checks it). The names are those of shell.md 3.21. A value that
is not allowed falls back to the default; a few values of Raye Settings'
first plan are read too (`iconLabel`, `whenFull`, `mainAndOpen`,
`morePins`, `moreRecommended`), but writers use the names in the file.

Keys added for other parts: `[Start] PinnedFolders` (folders of pins,
written by the Start menu), `[Start] RestartNeeded` (a reason id; a dot
on the power button while it is set), `[Notifications] LockBadges`.

## Tests

`ctest -R lib-` runs `shellsettings`, `updatestate`, `recentlyadded` and
`components`. `tests/gallery.qml` shows every piece:

    QML_IMPORT_PATH=<build>/qml:theme/qml qml6 shell/lib/tests/gallery.qml [-- --day]
