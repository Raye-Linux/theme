// The toasts of the session, bottom right above the clock, drawn with the
// clock applet's own card (ToastCard.qml). Up to three; a card that runs
// out goes to the notification history, as on Plasma.
import QtQuick
import QtQuick.Window
import org.kde.layershell as LayerShell
import org.kde.notificationmanager as NotificationManager
import org.raye.hypr
import org.raye.shell

Window {
    id: toasts

    property bool centreOpen: false
    property bool doNotDisturb: false

    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowDoesNotAcceptFocus
    width: Math.max(1, stack.width)
    height: Math.max(1, stack.height + ShellTokens.flyoutGap)
    visible: cards.count > 0 && !centreOpen
    // declared inside the shell's Item: without this Qt waits for that
    // item's window, which never shows, before showing this one
    transientParent: null

    LayerShell.Window.scope: "raye-toast"
    LayerShell.Window.layer: LayerShell.Window.LayerOverlay
    LayerShell.Window.anchors: LayerShell.Window.AnchorBottom | LayerShell.Window.AnchorRight
    LayerShell.Window.exclusionZone: 0
    LayerShell.Window.keyboardInteractivity: LayerShell.Window.KeyboardInteractivityOnDemand

    readonly property url cardUrl: Views.url("org.raye.clock", "toasts/ToastCard.qml")

    NotificationManager.Notifications {
        id: popups
        limit: 3
        showExpired: false
        showDismissed: false
        showJobs: true
        groupMode: NotificationManager.Notifications.GroupDisabled
        sortMode: NotificationManager.Notifications.SortByTypeAndUrgency
        sortOrder: Qt.AscendingOrder
        urgencies: toasts.doNotDisturb ? NotificationManager.Notifications.CriticalUrgency
                                       : NotificationManager.Notifications.CriticalUrgency
                                         | NotificationManager.Notifications.NormalUrgency
                                         | NotificationManager.Notifications.LowUrgency
        window: toasts
    }

    Column {
        id: stack
        anchors.bottom: parent.bottom
        anchors.bottomMargin: ShellTokens.flyoutGap
        anchors.right: parent.right
        spacing: 8

        Repeater {
            id: cards
            model: popups
            delegate: Loader {
                id: holder
                required property int index
                required property var model
                function row() { return popups.index(index, 0) }
                source: toasts.cardUrl
                onLoaded: {
                    const m = holder.model
                    const c = item
                    c.applicationName = m.applicationName || ""
                    c.applicationIconName = m.applicationIconName || ""
                    c.desktopEntry = m.desktopEntry || ""
                    c.notifyRcName = m.notifyRcName || ""
                    c.summary = m.summary || ""
                    c.body = m.body || ""
                    c.iconName = m.iconName || ""
                    c.image = m.image
                    c.urls = m.urls || []
                    c.actionNames = m.actionNames || []
                    c.actionLabels = m.actionLabels || []
                    c.hasDefaultAction = !!m.hasDefaultAction
                    c.critical = m.urgency === NotificationManager.Notifications.CriticalUrgency
                    c.isJob = m.type === NotificationManager.Notifications.JobType
                    c.timeoutMs = m.timeout > 0 ? m.timeout : ShellSettings.toastSeconds * 1000
                    c.hasReplyAction = !!m.hasReplyAction
                    c.replyActionLabel = m.replyActionLabel || ""
                    c.replyPlaceholderText = m.replyPlaceholderText || ""
                    c.replySubmitButtonText = m.replySubmitButtonText || ""
                    c.timeUp.connect(() => popups.expire(holder.row()))
                    c.closeRequested.connect(() => c.leave(() => popups.close(holder.row())))
                    c.defaultInvoked.connect(() => popups.invokeDefaultAction(holder.row(), NotificationManager.Notifications.Close))
                    c.actionInvoked.connect(name => popups.invokeAction(holder.row(), name, NotificationManager.Notifications.Close))
                    c.replied.connect(text => popups.reply(holder.row(), text, NotificationManager.Notifications.Close))
                    c.killRequested.connect(() => popups.killJob(holder.row()))
                    // the card slides in from the right once it is on screen
                    Qt.callLater(() => c.state = "shown")
                }
            }
        }
    }
}
