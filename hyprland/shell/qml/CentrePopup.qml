// The notification centre (Win+N, or a click on the clock): the clock
// applet's own Centre (notifications over the calendar), bottom right
// above the clock, fed from the shell's notification history.
import QtQuick
import org.kde.notificationmanager as NotificationManager
import org.raye.hypr
import org.raye.shell

import "hyprlogic.js" as Logic

Flyout {
    id: popup

    property date now: new Date()
    // the history the clock's badge counts
    property var badgeModel
    property int focusMinutes: 30
    layerName: "raye-centre"
    content: loader.item
    readonly property bool open: visible
    readonly property bool doNotDisturb: Logic.dndActive(settings.notificationsInhibitedUntil, now,
                                                         settings.notificationsInhibitedByApplication)

    onOutsideClicked: close()

    NotificationManager.Settings {
        id: settings
    }

    NotificationManager.Notifications {
        id: notifications
        showExpired: true
        showDismissed: true
        showJobs: true
        expandUnread: true
        groupMode: NotificationManager.Notifications.GroupApplicationsFlat
        sortMode: NotificationManager.Notifications.SortByTypeAndUrgency
        urgencies: NotificationManager.Notifications.CriticalUrgency | NotificationManager.Notifications.NormalUrgency
                   | (settings.lowPriorityHistory ? NotificationManager.Notifications.LowUrgency : 0)
    }

    // what the centre's buttons do, as in the clock applet
    QtObject {
        id: controller

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
            popup.close()
        }
        function invokeAction(row, actionId) {
            notifications.invokeAction(index(row), actionId, NotificationManager.Notifications.Close)
        }
        function killJob(row) {
            notifications.killJob(index(row))
        }
        function toggleDnd() {
            if (popup.doNotDisturb) {
                settings.notificationsInhibitedUntil = undefined
                settings.revokeApplicationInhibitions()
                ShellSettings.focusUntil = new Date(NaN)
            } else {
                const d = new Date()
                d.setFullYear(d.getFullYear() + 100)
                settings.notificationsInhibitedUntil = d
            }
            settings.save()
        }
        function startFocus(minutes) {
            const until = new Date(Date.now() + minutes * 60000)
            popup.focusMinutes = minutes
            ShellSettings.focusUntil = until
            settings.notificationsInhibitedUntil = until
            settings.save()
        }
        function stopFocus() {
            ShellSettings.focusUntil = new Date(NaN)
            settings.notificationsInhibitedUntil = undefined
            settings.save()
        }
        function requestAgenda(from, to) {}
        function openCalendar(day, isNew) {
            popup.close()
            Launcher.openUrl("raye-calendar:?" + (isNew ? "new=" : "day=") + day)
        }
    }

    Loader {
        id: loader
        focus: true
        active: false
        source: Views.url("org.raye.clock", "Centre.qml")
        // the cards' shadow margin sits outside the 12 px from the edges
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 12 - (item ? item.margin : 0)
        anchors.bottomMargin: ShellTokens.flyoutGap - (item ? item.margin : 0)
        onLoaded: {
            item.model = notifications
            item.controller = controller
            item.now = Qt.binding(() => popup.now)
            item.doNotDisturb = Qt.binding(() => popup.doNotDisturb)
            item.focusRemaining = Qt.binding(() => Logic.focusRemaining(ShellSettings.focusUntil, popup.now))
            item.focusMinutes = Qt.binding(() => popup.focusMinutes)
            item.requestClose.connect(popup.close)
            item.closed.connect(() => popup.visible = false)
        }
    }

    function show() {
        loader.active = true
        const c = loader.item
        if (!c)
            return
        c.calendarCard.selected = new Date()
        c.calendarCard.showToday()
        c.prepare(Math.max(300, popup.height - ShellTokens.flyoutGap - 2 * 12 - 2 * c.margin))
        c.shown = true
        visible = true
        c.animated = true
        requestActivate()
        if (badgeModel)
            badgeModel.lastRead = new Date()
    }
    function close() {
        if (visible && loader.item)
            loader.item.shown = false
    }
    function toggle() {
        if (visible)
            close()
        else
            show()
    }
}
