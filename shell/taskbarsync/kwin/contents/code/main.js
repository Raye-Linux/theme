// KWin knows first when a screen comes, goes or becomes the main one.
// It hands the work to raye-taskbar-sync.service, which waits for Plasma
// and puts the taskbars right. Also once at login.

function syncTaskbars() {
    callDBus("org.freedesktop.systemd1", "/org/freedesktop/systemd1",
             "org.freedesktop.systemd1.Manager", "StartUnit",
             "raye-taskbar-sync.service", "replace");
}

workspace.screensChanged.connect(syncTaskbars);
workspace.screenOrderChanged.connect(syncTaskbars);
syncTaskbars();
