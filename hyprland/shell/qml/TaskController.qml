// The taskbar's tasks on Hyprland: the model and the controller TaskBar.qml
// (from the Raye taskbar applet) works with, made from Hyprland's client
// list instead of Plasma's tasks model. One per taskbar.
import QtQuick
import org.raye.hypr
import org.raye.shell

import "hyprlogic.js" as Logic

Item {
    id: controller

    visible: false

    // the workspace the taskbar's screen shows
    property int workspaceId: 1
    // one button per app, or one per window (when the labels are shown)
    property bool grouped: true
    property bool showPins: true
    readonly property alias model: tasks
    property int revision: 0

    property var rows: []
    // the order the shell first saw each window in
    property var seen: ({})
    property int seenNext: 0

    ListModel { id: tasks }

    function rebuild() {
        const all = Hyprland.clients
        const clients = all.filter(c => Logic.isMinimized(c) || !c.workspace || c.workspace.id === workspaceId)
        const next = Object.assign({}, seen)
        for (let i = 0; i < clients.length; ++i) {
            if (!(clients[i].address in next))
                next[clients[i].address] = seenNext++
        }
        seen = next
        const pinned = showPins ? ShellSettings.taskbarPinned : []
        const made = []
        const found = Logic.taskRows(clients, Hyprland.activeAddress, pinned, AppInfo.forClients(clients),
                                    grouped, seen)
        for (let i = 0; i < found.length; ++i) {
            const r = found[i]
            if (r.AppName === "" && r.desktopId !== "") {
                const app = AppInfo.forDesktopId(r.desktopId)
                // the pin of an app that is not installed (any more) is not shown
                if (!app.id && r.IsLauncher)
                    continue
                r.AppName = app.name || ""
                r.display = r.display || r.AppName
                r.decoration = app.icon || r.decoration
            }
            made.push(r)
        }
        rows = made
        // rows with the same key keep their delegate, so a title or a
        // state change does not rebuild the button
        const keys = made.map(r => r.desktopId + "|" + r.windows.map(w => w.address).join(","))
        let same = tasks.count === made.length
        for (let i = 0; same && i < made.length; ++i)
            same = tasks.get(i).key === keys[i]
        if (!same)
            tasks.clear()
        for (let i = 0; i < made.length; ++i) {
            const r = {
                key: keys[i], display: made[i].display, decoration: made[i].decoration,
                AppName: made[i].AppName, AppId: made[i].AppId,
                LauncherUrlWithoutIcon: made[i].LauncherUrlWithoutIcon,
                IsLauncher: made[i].IsLauncher, IsStartup: false, IsActive: made[i].IsActive,
                IsMinimized: made[i].IsMinimized, IsDemandingAttention: made[i].IsDemandingAttention,
                ChildCount: made[i].ChildCount
            }
            if (same)
                tasks.set(i, r)
            else
                tasks.append(r)
        }
        revision++
    }

    Connections {
        target: Hyprland
        function onClientsChanged() { controller.rebuild() }
    }
    Connections {
        target: ShellSettings
        function onTaskbarChanged() { controller.rebuild() }
    }
    onGroupedChanged: rebuild()
    onWorkspaceIdChanged: rebuild()
    Component.onCompleted: rebuild()

    function row(i) { return rows[i] || null }
    function send(commands) {
        if (commands.length > 0)
            Hyprland.request(Logic.batch(commands))
    }

    // --- what TaskBar.qml calls ---------------------------------------------

    function counts() {
        const apps = {}
        let grouped = 0
        let ungrouped = 0
        for (let i = 0; i < rows.length; ++i) {
            const key = rows[i].desktopId || rows[i].AppId
            if (!apps[key]) {
                apps[key] = true
                ++grouped
            }
            ungrouped += Math.max(1, rows[i].windows.length)
        }
        return { grouped: grouped, ungrouped: ungrouped }
    }

    function launch(i) {
        const r = row(i)
        if (r && r.desktopId)
            Launcher.launch(r.desktopId)
    }
    function newInstance(i) { launch(i) }
    function activate(i) {
        const r = row(i)
        if (r && r.windows.length > 0)
            send(Logic.activateCommands(r.windows[0], workspaceId))
    }
    // a click on the app in use puts it away; on a minimized one, back up
    function minimize(i) {
        const r = row(i)
        if (!r || r.windows.length === 0)
            return
        if (r.IsMinimized)
            send(Logic.activateCommands(r.windows[0], workspaceId))
        else
            send([].concat.apply([], r.windows.map(w => Logic.minimizeCommands(w))))
    }
    function cycle(i) {
        const r = row(i)
        if (!r || r.windows.length === 0)
            return
        const active = r.windows.findIndex(w => w.active)
        send(Logic.activateCommands(r.windows[(active + 1) % r.windows.length], workspaceId))
    }
    function close(i) {
        const r = row(i)
        if (r)
            send([].concat.apply([], r.windows.map(w => Logic.closeCommands(w))))
    }
    function desktopId(i) {
        const r = row(i)
        return r ? r.desktopId : ""
    }
    function windowsOf(i) {
        const r = row(i)
        if (!r || r.IsLauncher)
            return []
        return r.windows.map((w, c) => ({ title: w.title, icon: r.decoration, winId: w.address,
                                          active: w.active, child: r.windows.length > 1 ? c : -1 }))
    }
    function activateWindow(i, child) {
        const r = row(i)
        if (r)
            send(Logic.activateCommands(r.windows[Math.max(0, child)], workspaceId))
    }
    function closeWindow(i, child) {
        const r = row(i)
        if (r)
            send(Logic.closeCommands(r.windows[Math.max(0, child)]))
    }
    function endTask(i) {
        const r = row(i)
        if (r && r.AppPid > 1 && ShellSettings.endTask)
            Launcher.endProcess(r.AppPid)
    }
    // dropping files on a button: nothing on Hyprland yet
    function openUrls(i, urls) {}

    function isPinned(i) {
        const r = row(i)
        return !!r && (r.IsLauncher || r.HasLauncher)
    }
    function togglePin(i) {
        const r = row(i)
        if (!r || !r.desktopId)
            return
        const pins = ShellSettings.taskbarPinned.slice()
        const at = pins.indexOf(r.desktopId)
        if (at >= 0)
            pins.splice(at, 1)
        else
            pins.push(r.desktopId)
        ShellSettings.taskbarPinned = pins
    }
    // a button dragged to a new place: pins keep the new order, the other
    // apps keep theirs for the rest of the session
    function move(from, to) {
        if (from < 0 || to < 0 || from >= rows.length || to >= rows.length || from === to)
            return
        const order = rows.slice()
        const moved = order.splice(from, 1)[0]
        order.splice(to, 0, moved)
        const pins = order.filter(r => (r.IsLauncher || r.HasLauncher) && r.desktopId).map(r => r.desktopId)
        const next = Object.assign({}, seen)
        let n = 0
        for (let i = 0; i < order.length; ++i) {
            for (let w = 0; w < order[i].windows.length; ++w)
                next[order[i].windows[w].address] = n++
        }
        seenNext = Math.max(seenNext, n)
        seen = next
        if (pins.join(",") !== ShellSettings.taskbarPinned.join(","))
            ShellSettings.taskbarPinned = pins
        else
            rebuild()
    }
}
