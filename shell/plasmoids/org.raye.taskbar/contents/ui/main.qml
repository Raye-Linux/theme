// org.raye.taskbar: the apps of the Raye taskbar. Pinned apps and open
// windows come from Plasma's tasks model (one button per app, pinned
// apps in place); the row itself is TaskBar.qml. Options are read from
// rayeshellrc through ShellSettings and followed live.
//
// Plasmashell's Meta+1...9 calls activateTaskAtIndex() on the applet that
// provides org.kde.plasma.multitasking: this one.
import QtQuick
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.taskmanager as TaskManager
import org.raye.shell

import "taskbarlogic.js" as Logic

PlasmoidItem {
    id: root

    preferredRepresentation: fullRepresentation
    Plasmoid.backgroundHints: PlasmaCore.Types.NoBackground
    Plasmoid.constraintHints: Plasmoid.CanFillArea
    Plasmoid.status: taskbarFocused ? PlasmaCore.Types.AcceptingInputStatus : PlasmaCore.Types.ActiveStatus
    // the taskbar is set up in Raye Settings, never in a Plasma dialog
    Plasmoid.hasConfigurationInterface: false

    property bool taskbarFocused: false

    // the Start applet right before the taskbar centres the whole group;
    // without one the taskbar centres its own row
    readonly property var startApplet: {
        const applets = Plasmoid.containment ? Plasmoid.containment.applets : []
        for (let i = 0; i < applets.length; ++i) {
            if (applets[i] && applets[i].pluginName === "org.raye.start")
                return applets[i]
        }
        return null
    }

    function activateTaskAtIndex(index) {
        if (root.fullRepresentationItem)
            root.fullRepresentationItem.activateTaskAtIndex(index)
    }

    // --- tasks -------------------------------------------------------------
    TaskManager.VirtualDesktopInfo { id: virtualDesktopInfo }
    TaskManager.ActivityInfo { id: activityInfo }

    TaskManager.TasksModel {
        id: tasksModel

        // a taskbar on a second screen shows only the windows on it, as
        // "Show my taskbar apps on" asks; the pins are then on the main
        // taskbar only
        readonly property bool mainScreen: Plasmoid.containment ? Plasmoid.containment.screen <= 0 : true
        readonly property bool showPins: Logic.showPins(ShellSettings.multiDisplayApps, mainScreen)
        onShowPinsChanged: if (ready) launcherList = wantedLaunchers()

        function wantedLaunchers() {
            return showPins ? Logic.launchersForPinned(ShellSettings.taskbarPinned) : []
        }

        virtualDesktop: virtualDesktopInfo.currentDesktop
        screenGeometry: Plasmoid.containment ? Plasmoid.containment.screenGeometry : Qt.rect(0, 0, 0, 0)
        activity: activityInfo.currentActivity
        filterByVirtualDesktop: true
        filterByActivity: true
        filterByScreen: ShellSettings.multiDisplayApps === "open"
                        || (ShellSettings.multiDisplayApps === "main-and-open" && !mainScreen)
        filterNotMinimized: false
        // Not true: on Wayland the model answers IsHidden with IsMinimized,
        // so a minimized window would leave the row and a click on its pin
        // would start a second copy of the app.
        filterHidden: false
        launchInPlace: true
        separateLaunchers: false
        hideActivatedLaunchers: true
        sortMode: TaskManager.TasksModel.SortManual
        groupMode: root.fullRepresentationItem && root.fullRepresentationItem.labels
                   ? TaskManager.TasksModel.GroupDisabled : TaskManager.TasksModel.GroupApplications
        groupInline: false

        Component.onCompleted: {
            launcherList = wantedLaunchers()
            ready = true
        }
        property bool ready: false

        // a pin added or removed here (the jump list, a drag) goes back
        // to rayeshellrc; one changed in Raye Settings comes in below
        onLauncherListChanged: {
            // a taskbar without the pins never writes them
            if (!ready || !showPins)
                return
            const ids = Logic.pinnedForLaunchers(launcherList)
            if (!Logic.sameList(ids, ShellSettings.taskbarPinned))
                ShellSettings.taskbarPinned = ids
        }

        onCountChanged: taskController.revision++
        onDataChanged: taskController.revision++
        onModelReset: taskController.revision++
    }

    Connections {
        target: ShellSettings
        function onTaskbarChanged() {
            const urls = tasksModel.wantedLaunchers()
            if (!Logic.sameList(urls, Logic.launchersForPinned(Logic.pinnedForLaunchers(tasksModel.launcherList))))
                tasksModel.launcherList = urls
        }
    }

    QtObject {
        id: taskController

        property int revision: 0

        function index(i) {
            return tasksModel.makeModelIndex(i)
        }
        function data(i, role) {
            return tasksModel.data(index(i), role)
        }

        function counts() {
            const apps = {}
            let grouped = 0
            let ungrouped = 0
            for (let i = 0; i < tasksModel.count; ++i) {
                const key = Logic.appKey(data(i, TaskManager.AbstractTasksModel.LauncherUrlWithoutIcon),
                                         data(i, TaskManager.AbstractTasksModel.AppId)) || ("#" + i)
                if (!apps[key]) {
                    apps[key] = true
                    ++grouped
                }
                ungrouped += Math.max(1, data(i, TaskManager.AbstractTasksModel.ChildCount) || 0)
            }
            return { grouped: grouped, ungrouped: ungrouped }
        }

        function launch(i) { tasksModel.requestActivate(index(i)) }
        function newInstance(i) { tasksModel.requestNewInstance(index(i)) }
        function activate(i) { tasksModel.requestActivate(index(i)) }
        function minimize(i) { tasksModel.requestToggleMinimized(index(i)) }
        function close(i) { tasksModel.requestClose(index(i)) }

        // the app's own .desktop id ("" for a window with none)
        function desktopId(i) {
            return Logic.desktopIdForLauncher(data(i, TaskManager.AbstractTasksModel.LauncherUrlWithoutIcon))
        }

        // the windows behind a button, for the hover preview:
        // [{ title, icon, winId, active, child }] (child -1: the button itself)
        function windowsOf(i) {
            if (i < 0 || i >= tasksModel.count || data(i, TaskManager.AbstractTasksModel.IsLauncher))
                return []
            const one = (idx, child) => ({
                title: String(tasksModel.data(idx, Qt.DisplayRole) || ""),
                icon: tasksModel.data(idx, Qt.DecorationRole),
                winId: (tasksModel.data(idx, TaskManager.AbstractTasksModel.WinIdList) || [])[0] || "",
                active: !!tasksModel.data(idx, TaskManager.AbstractTasksModel.IsActive),
                child
            })
            const n = data(i, TaskManager.AbstractTasksModel.ChildCount) || 0
            if (n <= 0)
                return [one(index(i), -1)]
            const out = []
            for (let c = 0; c < n; ++c)
                out.push(one(tasksModel.makeModelIndex(i, c), c))
            return out
        }
        function windowIndex(i, child) {
            return child < 0 ? index(i) : tasksModel.makeModelIndex(i, child)
        }
        function activateWindow(i, child) { tasksModel.requestActivate(windowIndex(i, child)) }
        function closeWindow(i, child) { tasksModel.requestClose(windowIndex(i, child)) }
        // "End task": stop the app's process (only when Settings allows it)
        function endTask(i) {
            const pid = Number(data(i, TaskManager.AbstractTasksModel.AppPid) || 0)
            if (pid > 1 && ShellSettings.endTask)
                Launcher.endProcess(pid)
        }
        function openUrls(i, urls) { tasksModel.requestOpenUrls(index(i), urls) }

        function cycle(i) {
            const n = data(i, TaskManager.AbstractTasksModel.ChildCount) || 0
            let active = -1
            for (let c = 0; c < n; ++c) {
                if (tasksModel.data(tasksModel.makeModelIndex(i, c), TaskManager.AbstractTasksModel.IsActive)) {
                    active = c
                    break
                }
            }
            const next = Logic.nextInGroup(n, active)
            if (next >= 0)
                tasksModel.requestActivate(tasksModel.makeModelIndex(i, next))
        }

        function move(from, to) {
            if (tasksModel.move(from, to))
                tasksModel.syncLaunchers()
        }

        function isPinned(i) {
            return !!data(i, TaskManager.AbstractTasksModel.IsLauncher)
                || !!data(i, TaskManager.AbstractTasksModel.HasLauncher)
        }

        function togglePin(i) {
            const url = data(i, TaskManager.AbstractTasksModel.LauncherUrlWithoutIcon)
            if (!url)
                return
            if (isPinned(i))
                tasksModel.requestRemoveLauncher(url)
            else
                tasksModel.requestAddLauncher(url)
        }
    }

    // --- badges: each app's own unread count ------------------------------
    //
    // These used to be notification counts: the number on an icon was
    // "how many of this app's notifications are still in the centre". It
    // was not a badge. Clearing the notification centre cleared every
    // badge while the mail was still unread, and reading a message in
    // the app left the badge where it was, because its notification was
    // still in the centre.
    //
    // They now come from com.canonical.Unity.LauncherEntry, where an
    // application publishes its OWN count -- what Telegram, Discord,
    // Thunderbird, Nextcloud and the GNOME/Elementary mail clients
    // already emit. LauncherEntryWatcher is the pipe; what the numbers
    // mean is decided in taskbarlogic.js.
    LauncherEntryWatcher {
        // only while badges are on: the option switches the watcher off,
        // it does not merely hide the circles
        active: ShellSettings.badges
        onActiveChanged: if (!active) root.badgeCounts = ({})
        onEntryUpdate: (appUri, properties) => {
            root.badgeCounts = Logic.applyLauncherEntry(root.badgeCounts, appUri, properties)
        }
        // the application is gone and will say nothing more: its badge
        // goes with it. A number nobody can clear sends someone into an
        // app to look for a message that is not there.
        onEntryGone: appUri => {
            root.badgeCounts = Logic.dropLauncherEntry(root.badgeCounts, appUri)
        }
    }
    property var badgeCounts: ({})

    // --- Meta+T ------------------------------------------------------------
    GlobalShortcut {
        name: "raye-focus-taskbar"
        text: i18nd("raye_shell_taskbar", "Move the focus to the taskbar")
        sequence: "Meta+T"
        onActivated: {
            root.taskbarFocused = true
            if (root.fullRepresentationItem)
                root.fullRepresentationItem.takeFocus()
        }
    }

    // the row's width, for the Start applet's centring (only written when it changes)
    Timer {
        id: publishTimer
        interval: 200
        onTriggered: {
            const w = root.fullRepresentationItem ? root.fullRepresentationItem.rowWidth : 0
            if (Plasmoid.configuration.rowWidth !== w)
                Plasmoid.configuration.rowWidth = w
        }
    }

    // right-click on the free space of the taskbar: what Windows offers
    // there. Plasma's own menu (its settings) never shows.
    // made the first time it opens
    Loader {
        id: taskbarMenuLoader
        active: false
        sourceComponent: PanelMenu {
            RayeMenuItem {
                text: i18nd("raye_shell_taskbar", "Task Manager")
                icon.name: "utilities-system-monitor"
                // Raye's own first, the KDE monitor only if it is not
                // there: raye-taskmanager is a package of its own and may
                // be absent. launch() rends false for a missing app and
                // nothing listens to Launcher.failed, so the fallback is
                // silent -- and a menu item that does nothing is worse
                // than one that is not shown.
                onTriggered: Launcher.launch("org.raye.taskmanager.desktop")
                             || Launcher.launch("org.kde.plasma-systemmonitor.desktop")
            }
            RayeMenuSeparator {}
            RayeMenuItem {
                text: i18nd("raye_shell_taskbar", "Taskbar settings")
                icon.name: "configure"
                onTriggered: Launcher.openSettings("personalization/taskbar")
            }
        }
    }

    fullRepresentation: TaskBar {
        id: taskBarItem
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumWidth: ShellTokens.buttonPitch
        // not rowWidth: that one follows the width the panel gives
        Layout.preferredWidth: buttonCount * ShellTokens.buttonPitch

        model: tasksModel
        // not "controller: controller": inside TaskBar that name is its own
        // property, so the row would get nothing
        controller: taskController
        badgeCounts: root.badgeCounts
        groupedWithStart: root.startApplet !== null

        onRowWidthChanged: publishTimer.restart()
        onEscaped: root.taskbarFocused = false
        onEmptyMenuRequested: (x, y) => {
            taskbarMenuLoader.active = true
            taskbarMenuLoader.item.open(taskBarItem, x)
        }
        onActiveFocusChanged: if (!activeFocus) root.taskbarFocused = false
    }
}
