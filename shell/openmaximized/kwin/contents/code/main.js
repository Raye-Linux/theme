// An app opens maximized: its FIRST window only. A window an open app shows
// next (File Explorer's Properties or Options, an app's settings) keeps its
// own size, even when the app made it a separate window rather than a
// dialog. Only real app windows: a normal window
// that can be resized and maximized, not a dialog of another window, not
// one kept off the taskbar (the shell's own menus and popups), not the
// shell itself, and not one whose largest size is smaller than the screen
// (a fixed-size welcome window stays centred, where the window rule puts it).
// Once shown, the person restores or resizes it as they like.

function wantsMaximized(w) {
    if (!w || !w.normalWindow || w.transient || w.specialWindow || w.skipTaskbar)
        return false;
    if (!w.resizeable || !w.maximizable)
        return false;
    const app = String(w.resourceClass || "").toLowerCase();
    if (app === "plasmashell" || app === "org.kde.plasmashell" || app === "ksmserver" || app === "krunner")
        return false;
    // the app already has a window on screen: this one is a second window
    const others = (workspace.windowList ? workspace.windowList() : []).filter(o =>
        o !== w && o.normalWindow && !o.skipTaskbar
        && ((w.pid > 0 && o.pid === w.pid) || (app !== "" && String(o.resourceClass || "").toLowerCase() === app)));
    if (others.length > 0)
        return false;
    const area = workspace.clientArea(KWin.MaximizeArea, w);
    const most = w.maxSize;
    if (most && ((most.width > 0 && most.width < area.width) || (most.height > 0 && most.height < area.height)))
        return false;
    return true;
}

workspace.windowAdded.connect(function (w) {
    if (wantsMaximized(w))
        w.setMaximize(true, true);
});
