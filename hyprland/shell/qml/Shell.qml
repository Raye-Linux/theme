// The Raye shell on Hyprland: a taskbar and a wallpaper on every screen,
// one Start, one quick settings panel and the toasts, and what the keys
// bound in hyprland.conf ask for (ShellBus).
import QtQuick
import QtQuick.Window
import org.kde.notificationmanager as NotificationManager
import org.raye.hypr
import org.raye.shell

import "hyprlogic.js" as Logic

Item {
    id: hyprShell

    visible: false

    property date now: new Date()
    readonly property bool twentyFour: ShellSettings.clockUse24h === "24"
                                       || (ShellSettings.clockUse24h !== "12"
                                           && Qt.locale().timeFormat(Locale.ShortFormat).indexOf("AP") < 0
                                           && Qt.locale().timeFormat(Locale.ShortFormat).indexOf("ap") < 0)
    readonly property bool startOpen: start.open
    readonly property string startMode: start.mode
    readonly property bool quickOpen: quick.open
    readonly property bool centreOpen: centre.open
    readonly property int unreadCount: history.unreadNotificationsCount
    // the network, volume and battery state, shared by the pill and the panel
    readonly property var system: systemLoader.item

    Timer {
        interval: 1000 - new Date().getMilliseconds()
        running: true
        repeat: true
        onTriggered: {
            hyprShell.now = new Date()
            interval = 1000 - hyprShell.now.getMilliseconds()
        }
    }

    Loader {
        id: systemLoader
        source: Views.url("org.raye.quicksettings", "Backend.qml")
    }

    NotificationManager.Notifications {
        id: history
        showExpired: true
        showDismissed: true
    }

    // --- screens -------------------------------------------------------------

    function monitorFor(screen) {
        const ms = Hyprland.monitors
        for (let i = 0; i < ms.length; ++i) {
            if (screen && ms[i].name === screen.name)
                return ms[i]
        }
        return ms.length > 0 ? ms[0] : null
    }

    Instantiator {
        id: panels
        model: Qt.application.screens
        delegate: Panel {
            required property var modelData
            required property int index
            screen: modelData
            shell: hyprShell
            main: index === 0
            workspaceId: {
                const m = hyprShell.monitorFor(modelData)
                return m && m.activeWorkspace ? m.activeWorkspace.id : Hyprland.activeWorkspace
            }
        }
    }
    Instantiator {
        model: Qt.application.screens
        delegate: Wallpaper {
            required property var modelData
            required property int index
            screen: modelData
            screenIndex: index
        }
    }

    // the taskbar on the screen with the focus
    function activePanel() {
        const m = Hyprland.focusedMonitor()
        for (let i = 0; i < panels.count; ++i) {
            const p = panels.objectAt(i)
            if (p && p.screen && m && p.screen.name === m.name)
                return p
        }
        return panels.count > 0 ? panels.objectAt(0) : null
    }

    StartPopup { id: start }
    QuickPopup {
        id: quick
        system: hyprShell.system
    }
    CentrePopup {
        id: centre
        now: hyprShell.now
        badgeModel: history
    }
    Toasts {
        centreOpen: centre.open
        doNotDisturb: centre.doNotDisturb
    }

    function toggle(what, panel) {
        panel = panel || activePanel()
        if (panel) {
            start.screen = panel.screen
            quick.screen = panel.screen
        }
        switch (what) {
        case "start":
        case "search":
        case "quicklinks":
            quick.close()
            centre.close()
            start.toggle(what === "search" ? "search" : "start", panel ? panel.startButton : null)
            break
        case "quick":
            start.close()
            centre.close()
            quick.toggle()
            break
        case "centre":
            start.close()
            quick.close()
            if (panel)
                centre.screen = panel.screen
            centre.toggle()
            break
        }
    }

    // --- Win+arrows, Win+D ---------------------------------------------------

    // where each window was before its first snap, and how it is snapped now
    property var snapped: ({})
    function snap(direction) {
        const w = Hyprland.activeWindow()
        if (!w || !w.address)
            return
        const area = Logic.workArea(Hyprland.focusedMonitor())
        const state = snapped[w.address] || { mode: "", restore: null }
        const maximized = w.fullscreen === 1
        const mode = maximized ? "maximized" : state.mode
        const step = Logic.snapStep(mode, direction)
        let restore = state.restore
        if (!restore && (step.action === "snap" || step.action === "maximize"))
            restore = { x: w.at[0], y: w.at[1], width: w.size[0], height: w.size[1] }
        Hyprland.request(Logic.batch(Logic.snapCommands(step, w, area, restore, maximized)))
        const next = Object.assign({}, snapped)
        if (step.action === "restore" || step.action === "minimize")
            delete next[w.address]
        else
            next[w.address] = { mode: step.mode, restore: restore }
        snapped = next
    }

    property var desktopHidden: []
    function showDesktop() {
        const r = Logic.showDesktop(Hyprland.clients, Hyprland.activeWorkspace, desktopHidden)
        desktopHidden = r.hidden
        if (r.commands.length > 0)
            Hyprland.request(Logic.batch(r.commands))
    }
    // a window opened or brought up by hand ends "show desktop"
    Connections {
        target: Hyprland
        function onEvent(name, data) {
            if (name === "openwindow" || name === "activewindowv2" && data !== "")
                hyprShell.desktopHidden = []
            if (name === "closewindow" && hyprShell.snapped["0x" + data]) {
                const next = Object.assign({}, hyprShell.snapped)
                delete next["0x" + data]
                hyprShell.snapped = next
            }
        }
    }

    function closeDesktop() {
        const ws = Hyprland.activeWorkspace
        const to = ws > 1 ? ws - 1 : ws + 1
        const commands = Hyprland.clients.filter(c => c.workspace && c.workspace.id === ws)
                                         .map(c => "dispatch movetoworkspacesilent " + to + ",address:" + c.address)
        commands.push("dispatch workspace " + to)
        Hyprland.request(Logic.batch(commands))
    }

    Connections {
        target: ShellBus
        function onToggle(what) { hyprShell.toggle(what, null) }
        function onSnap(direction) { hyprShell.snap(direction) }
        function onDesktop() { hyprShell.showDesktop() }
        function onCloseDesktop() { hyprShell.closeDesktop() }
        function onMinimize(pid, title) {
            const w = Logic.windowOf(Hyprland.clients, pid, title, Hyprland.activeAddress)
            if (w)
                Hyprland.request(Logic.batch(Logic.minimizeCommands(w)))
        }
        function onTask(number) {
            const p = hyprShell.activePanel()
            if (p && p.taskBarItem)
                p.taskBarItem.activateTaskAtIndex(number - 1)
        }
    }
}
