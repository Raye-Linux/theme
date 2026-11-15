// org.raye.clock: the clock and the notification bell of the Raye
// taskbar, and the notification centre with the calendar they open
// (click, or the applet's Win+N). This applet starts the notification
// server of the session, once per plasmashell. The clock of the main
// screen also draws its pop-ups (toasts/Toasts.qml); raye-taskbar-sync takes
// the stock notifications applet out of the tray so the two never compete.
import QtQuick
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.kde.notificationmanager as NotificationManager
import org.raye.shell

import "clocklogic.js" as Logic
import "toasts/toastlogic.js" as ToastLogic

PlasmoidItem {
    id: root

    // the clock button is the applet's only face: Plasma never builds a
    // compact representation for an applet that has no full one, so the
    // button is given as the full representation and the centre is a
    // window of its own
    preferredRepresentation: fullRepresentation
    activationTogglesExpanded: false
    Plasmoid.backgroundHints: PlasmaCore.Types.NoBackground
    Plasmoid.status: centreWindow.visible ? PlasmaCore.Types.RequiresAttentionStatus : PlasmaCore.Types.ActiveStatus
    // set up in Raye Settings, never in a Plasma dialog
    Plasmoid.hasConfigurationInterface: false
    toolTipMainText: ""
    toolTipSubText: ""

    property date now: new Date()
    readonly property var screenRect: Plasmoid.containment ? Plasmoid.containment.screenGeometry : Qt.rect(0, 0, 1280, 800)
    readonly property bool twentyFour: Logic.use24h(ShellSettings.clockUse24h, Qt.locale().timeFormat(Locale.ShortFormat))
    readonly property bool doNotDisturb: Logic.dndActive(notificationSettings.notificationsInhibitedUntil, now,
                                                         notificationSettings.notificationsInhibitedByApplication)
    readonly property int focusRemaining: Logic.focusRemaining(ShellSettings.focusUntil, now)
    readonly property string longDate: now.toLocaleDateString(Qt.locale(), Locale.LongFormat)

    // --- the time --------------------------------------------------------
    Timer {
        id: tick
        running: true
        repeat: false
        interval: Logic.nextTick(new Date(), ShellSettings.clockShowSeconds || root.focusRemaining > 0)
        onTriggered: {
            root.now = new Date()
            interval = Logic.nextTick(root.now, ShellSettings.clockShowSeconds || root.focusRemaining > 0)
            restart()
        }
    }
    Connections {
        target: ShellSettings
        function onClockChanged() { root.now = new Date(); tick.restart() }
        function onFocusChanged() { root.now = new Date(); tick.restart() }
    }

    // --- the notification server ------------------------------------------
    // started by the applet's C++ part (clockapplet.cpp) before this loads
    property bool serverStarted: false
    Component.onCompleted: {
        serverStarted = true
        const last = Plasmoid.configuration.lastRead ? new Date(Plasmoid.configuration.lastRead) : null
        if (last && !isNaN(last.getTime()))
            notifications.lastRead = last
    }

    NotificationManager.Settings {
        id: notificationSettings
    }

    NotificationManager.Notifications {
        id: notifications
        showExpired: true
        showDismissed: true
        showJobs: true
        expandUnread: true
        groupMode: NotificationManager.Notifications.GroupApplicationsFlat
        sortMode: NotificationManager.Notifications.SortByTypeAndUrgency
        blacklistedDesktopEntries: Logic.historyBlacklist(notificationSettings.historyBlacklistedApplications)
        blacklistedNotifyRcNames: notificationSettings.historyBlacklistedServices.concat(ToastLogic.NEVER_SHOWN)
        urgencies: NotificationManager.Notifications.CriticalUrgency | NotificationManager.Notifications.NormalUrgency
                   | (notificationSettings.lowPriorityHistory ? NotificationManager.Notifications.LowUrgency : 0)
    }

    Loader {
        id: toasts
        // one set of toasts, on the main screen: every screen has a clock
        active: root.serverStarted && Plasmoid.ownsToasts
        source: "toasts/Toasts.qml"
        onLoaded: root.updateToasts()
    }
    function updateToasts() {
        const t = toasts.item
        if (!t)
            return
        t.screenGeometry = root.screenRect
        t.panelTop = root.panelTop()
        t.centreOpen = centreWindow.visible
        t.doNotDisturb = root.doNotDisturb
        const face = root.fullRepresentationItem
        t.window = face ? face.Window.window : null
    }
    onScreenRectChanged: updateToasts()
    onDoNotDisturbChanged: updateToasts()

    function panelTop() {
        const c = root.fullRepresentationItem
        return c && c.Window.window ? c.Window.window.y : screenRect.y + screenRect.height - ShellTokens.taskbarHeight
    }

    Loader {
        id: stockBell
        active: false
        source: "StockBell.qml"
    }
    Connections {
        target: Plasmoid
        function onStockBellTaken() { stockBell.active = true }
    }

    // --- what the centre's buttons do ----------------------------------------
    QtObject {
        id: centreController

        function index(row) {
            return notifications.index(row, 0)
        }
        function close(row) {
            notifications.close(index(row))
        }
        function clearAll() {
            notifications.clear(NotificationManager.Notifications.ClearExpired)
            for (let row = notifications.count - 1; row >= 0; --row) {
                const idx = index(row)
                if (notifications.data(idx, NotificationManager.Notifications.TypeRole) === NotificationManager.Notifications.NotificationType
                    && !notifications.data(idx, NotificationManager.Notifications.IsGroupRole))
                    notifications.close(idx)
            }
        }
        function invokeDefault(row) {
            notifications.invokeDefaultAction(index(row), NotificationManager.Notifications.Close)
            root.closeCentre()
        }
        function invokeAction(row, actionId) {
            notifications.invokeAction(index(row), actionId, NotificationManager.Notifications.Close)
        }
        function killJob(row) {
            notifications.killJob(index(row))
        }
        function toggleDnd() {
            if (root.doNotDisturb) {
                notificationSettings.notificationsInhibitedUntil = undefined
                notificationSettings.revokeApplicationInhibitions()
                if (root.focusRemaining > 0)
                    ShellSettings.focusUntil = new Date(NaN)
            } else {
                // until turned off: Plasma reads a date far ahead as "no end"
                const d = new Date()
                d.setFullYear(d.getFullYear() + 100)
                notificationSettings.notificationsInhibitedUntil = d
            }
            notificationSettings.save()
        }
        function startFocus(minutes) {
            const until = Logic.focusEnd(new Date(), minutes)
            Plasmoid.configuration.focusMinutes = minutes
            ShellSettings.focusUntil = until
            notificationSettings.notificationsInhibitedUntil = until
            notificationSettings.save()
            root.now = new Date()
        }
        // Raye Calendar, through its tool and its raye-calendar: links
        function requestAgenda(from, to) {
            Plasmoid.requestAgenda(from, to)
        }
        function openCalendar(day, isNew) {
            root.closeCentre()
            Launcher.openUrl("raye-calendar:?" + (isNew ? "new=" : "day=") + day)
        }
        function stopFocus() {
            ShellSettings.focusUntil = new Date(NaN)
            notificationSettings.notificationsInhibitedUntil = undefined
            notificationSettings.save()
            root.now = new Date()
        }
    }

    // --- Raye Calendar ----------------------------------------------------------
    property var calendarEvents: []
    Connections {
        target: Plasmoid
        function onAgendaReady(text) { root.calendarEvents = Logic.parseAgenda(text) }
    }

    // --- opening and closing the centre ---------------------------------------
    // made the first time it opens: most sessions never open it
    readonly property Item centre: centreLoader.item

    function openCentre() {
        centreLoader.active = true
        // back on today, and what may have changed in the calendar since
        centre.calendarCard.selected = new Date()
        centre.calendarCard.showToday()
        centre.calendarCard.wantRange()
        centre.shown = true
        const face = root.fullRepresentationItem
        const area = Plasmoid.moveToClockScreen(centreWindow, face ? face.Window.window : null)
        placement.open(screenRect, root.panelTop(), area)
        centreWindow.visible = true
        centre.animated = true
        centreWindow.requestActivate()
        notifications.lastRead = new Date()
        Plasmoid.configuration.lastRead = notifications.lastRead.toISOString()
        updateToasts()
    }
    function placeCentre() {
        if (centre)
            placement.fit(screenRect, root.panelTop())
    }
    CentrePlacement {
        id: placement
        window: centreWindow
        holder: centreHolder
        centre: root.centre
    }
    // the window got less height than the centre asked for (the screen
    // said it had more room than it does): fit the centre in what it got
    Timer {
        id: windowCheck
        interval: 250
        onTriggered: {
            if (centreWindow.visible && root.centre && centreWindow.height > 0
                    && centreWindow.height + 1 < root.centre.height) {
                root.centre.prepare(Math.max(300, centreWindow.height - 2 * root.centre.margin))
                root.centre.animated = true
                root.placeCentre()
            }
        }
    }
    function closeCentre() {
        if (centreWindow.visible && centre)
            centre.shown = false
    }
    function toggleCentre() {
        if (centreWindow.visible)
            closeCentre()
        else
            openCentre()
    }

    Connections {
        target: Plasmoid
        function onActivated() { root.toggleCentre() }
    }

    PlasmaCore.Dialog {
        id: centreWindow
        location: PlasmaCore.Types.Floating
        type: PlasmaCore.Dialog.AppletPopup
        backgroundHints: PlasmaCore.Dialog.NoBackground
        hideOnWindowDeactivate: true
        visible: false
        onVisibleChanged: {
            if (!visible && root.centre)
                root.centre.shown = false
            else
                windowCheck.restart()
            root.updateToasts()
        }
        onHeightChanged: if (visible) windowCheck.restart()

        // sized by CentrePlacement, never by the dialog's own guesses
        mainItem: FocusScope {
            id: centreHolder
            focus: true

            Loader {
                id: centreLoader
                // a Loader is a focus scope: without this the centre never
                // gets the keys of its window, and Esc does not close it
                focus: true
                active: false
                sourceComponent: Centre {
                    model: notifications
                    controller: centreController
                    now: root.now
                    doNotDisturb: root.doNotDisturb
                    focusRemaining: root.focusRemaining
                    focusMinutes: Plasmoid.configuration.focusMinutes
                    calendarAvailable: Plasmoid.calendarAvailable
                    calendarEvents: root.calendarEvents
                    calendarCollapsed: Plasmoid.configuration.calendarCollapsed
                    onCollapseToggled: Plasmoid.configuration.calendarCollapsed = !Plasmoid.configuration.calendarCollapsed
                    onFocusMinutesChanged: if (focusMinutes !== Plasmoid.configuration.focusMinutes) Plasmoid.configuration.focusMinutes = focusMinutes
                    onRequestClose: root.closeCentre()
                    onClosed: centreWindow.visible = false
                    onHeightChanged: if (centreWindow.visible) { root.placeCentre(); windowCheck.restart() }
                }
            }
        }
    }

    // right-click on the clock: what Windows offers there
    // made the first time it opens
    Loader {
        id: clockMenuLoader
        active: false
        sourceComponent: PanelMenu {
            RayeMenuItem {
                text: i18nd("raye_shell_clock", "Adjust date and time")
                icon.name: "view-calendar"
                onTriggered: Launcher.openSettings("time/date-time")
            }
            RayeMenuItem {
                text: i18nd("raye_shell_clock", "Notification settings")
                icon.name: "notifications"
                onTriggered: Launcher.openSettings("system/notifications")
            }
            RayeMenuSeparator {}
            RayeMenuItem {
                text: i18nd("raye_shell_clock", "Task Manager")
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
            RayeMenuItem {
                text: i18nd("raye_shell_clock", "Taskbar settings")
                icon.name: "configure"
                onTriggered: Launcher.openSettings("personalization/taskbar")
            }
        }
    }

    fullRepresentation: Item {
        id: clockFace
        Layout.minimumWidth: implicitWidth
        Layout.preferredWidth: implicitWidth
        Layout.fillHeight: true
        implicitWidth: clockButton.implicitWidth + (corner.visible ? corner.width : 0)
        implicitHeight: clockButton.implicitHeight
        // the clock tells the centre where the taskbar starts
        readonly property alias button: clockButton

        ClockButton {
            id: clockButton
            anchors.verticalCenter: parent.verticalCenter
            now: root.now
            twentyFour: root.twentyFour
            showSeconds: ShellSettings.clockShowSeconds
            showDate: ShellSettings.clockShowDate
            unreadCount: centreWindow.visible ? 0 : notifications.unreadNotificationsCount
            doNotDisturb: root.doNotDisturb
            focusFraction: Logic.focusFraction(root.focusRemaining, Plasmoid.configuration.focusMinutes)
            selected: centreWindow.visible || (clockMenuLoader.item !== null && clockMenuLoader.item.opened)
            longDate: root.longDate
            onClicked: root.toggleCentre()

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.RightButton
                onClicked: mouse => {
                    clockMenuLoader.active = true
                    clockMenuLoader.item.open(clockButton, mouse.x)
                }
            }
        }

        // "Select the far corner of the taskbar to show the desktop": a
        // thin strip at the very end, a line shows on hover
        ShowDesktopCorner {
            id: corner
            visible: ShellSettings.showDesktopCorner
            anchors.left: clockButton.right
            height: parent.height
        }
    }
}
