// The toasts of the session, drawn by the clock on the main screen. The
// clock loads this once, after it has started the notification server,
// and sets:
//   screenGeometry   the screen of the taskbar (screen coordinates)
//   panelTop         the top edge of the taskbar on that screen
//   centreOpen       the notification centre is open (no toasts then)
//   window           the taskbar's window, for activating apps
// Up to three cards above the clock, the newest at the bottom; the oldest
// goes to the centre when a fourth arrives. Do not disturb and Focus are
// decided here too: this is where the server learns about them.
import QtQuick
import org.kde.plasma.plasmoid
import org.kde.notificationmanager as NotificationManager
import org.kde.taskmanager as TaskManager
import org.raye.shell

import "toastlogic.js" as Logic

Item {
    id: toasts

    property rect screenGeometry
    property int panelTop: 0
    property bool centreOpen: false
    property var window: null
    // the centre's Do not disturb; the rest of the decision is below
    property bool doNotDisturb: false

    visible: false

    onCentreOpenChanged: {
        if (centreOpen)
            hideAll()
    }

    // --- do not disturb ----------------------------------------------------
    property date now: new Date()
    Timer {
        interval: 20000
        running: true
        repeat: true
        onTriggered: toasts.now = new Date()
    }
    NotificationManager.Settings {
        id: settings
        onNotificationsInhibitedUntilChanged: toasts.now = new Date()
    }
    readonly property bool inhibited: Logic.inhibited({
        serverValid: NotificationManager.Server.valid,
        until: settings.notificationsInhibitedUntil,
        now: toasts.now,
        byApplication: settings.notificationsInhibitedByApplication,
        whenFullscreen: settings.inhibitNotificationsWhenFullscreen,
        fullscreenFocused: settings.fullscreenFocused,
        whenMirrored: settings.inhibitNotificationsWhenScreensMirrored,
        screensMirrored: settings.screensMirrored
    })
    Component.onCompleted: applyInhibition()
    onInhibitedChanged: applyInhibition()

    function applyInhibition() {
        // the server first, so a summary sent now is not "added while inhibited"
        NotificationManager.Server.inhibited = inhibited
        if (!inhibited) {
            popups.showInhibitionSummary(settings.lowPriorityHistory ? NotificationManager.Notifications.LowUrgency
                                                                     : NotificationManager.Notifications.NormalUrgency,
                                         settings.historyBlacklistedApplications, settings.historyBlacklistedServices)
        }
        // the sounds follow inhibited on their own (ToastSounds.qml)
    }

    Loader {
        id: sounds
        source: "ToastSounds.qml"
        onLoaded: {
            // inhibited first: given the settings, it acts at once
            item.inhibited = Qt.binding(() => toasts.inhibited)
            item.settings = settings
        }
    }

    // --- what shows ----------------------------------------------------------
    NotificationManager.Notifications {
        id: popups
        limit: Logic.MAX_VISIBLE
        showExpired: false
        showDismissed: false
        showAddedDuringInhibition: false
        blacklistedDesktopEntries: settings.popupBlacklistedApplications
        blacklistedNotifyRcNames: settings.popupBlacklistedServices.concat(Logic.NEVER_SHOWN)
        whitelistedDesktopEntries: toasts.inhibited ? settings.doNotDisturbPopupWhitelistedApplications : []
        whitelistedNotifyRcNames: toasts.inhibited ? settings.doNotDisturbPopupWhitelistedServices : []
        showJobs: settings.jobsInNotifications
        sortMode: NotificationManager.Notifications.SortByTypeAndUrgency
        // the newest first, so the newest three are the ones shown
        sortOrder: Qt.DescendingOrder
        groupMode: NotificationManager.Notifications.GroupDisabled
        window: toasts.window
        urgencies: Logic.urgencies({
            inhibited: toasts.inhibited,
            criticalInDnd: settings.criticalPopupsInDoNotDisturbMode,
            lowPopups: settings.lowPriorityPopups,
            critical: NotificationManager.Notifications.CriticalUrgency,
            normal: NotificationManager.Notifications.NormalUrgency,
            low: NotificationManager.Notifications.LowUrgency
        })
    }

    // for a click on a notification that has no action of its own: its
    // app's window comes up
    TaskManager.TasksModel {
        id: tasks
        groupMode: TaskManager.TasksModel.GroupDisabled
    }
    function activateWindowOf(desktopEntry) {
        if (!desktopEntry)
            return false
        for (let i = 0; i < tasks.rowCount(); ++i) {
            const idx = tasks.index(i, 0)
            if (tasks.data(idx, TaskManager.AbstractTasksModel.AppId) === desktopEntry + ".desktop") {
                tasks.requestActivate(idx)
                return true
            }
        }
        return false
    }

    // --- placing them ----------------------------------------------------------
    ToastStack {
        id: stack
        windows: popupWindows
        // the panel may not be placed yet: the first screen then
        area: toasts.screenGeometry.width > 0 ? toasts.screenGeometry
                                              : Qt.rect(Application.screens[0].virtualX, Application.screens[0].virtualY,
                                                        Application.screens[0].width, Application.screens[0].height)
        panelTop: toasts.panelTop
        hidden: toasts.centreOpen
    }

    function place() {
        stack.place()
    }

    // a notification that changes rank (a job that ends, a new urgency)
    // moves in the list without leaving it
    Connections {
        target: popups
        function onRowsMoved() { Qt.callLater(toasts.place) }
        function onLayoutChanged() { Qt.callLater(toasts.place) }
    }

    // Through the model, last row first: each row hidden leaves the list,
    // the next notification steps in and the toasts are made anew, so a
    // toast picked before the loop may be gone by the time it is reached.
    function hideAll() {
        for (let row = popups.count - 1; row >= 0; --row) {
            const idx = popups.index(row, 0)
            const job = popups.data(idx, NotificationManager.Notifications.TypeRole) === NotificationManager.Notifications.JobType
            popups.setData(idx, true, job ? NotificationManager.Notifications.DismissedRole
                                          : NotificationManager.Notifications.ExpiredRole)
        }
    }

    Instantiator {
        id: popupWindows
        model: popups
        delegate: ToastWindow {
            id: popup

            required property int index
            required property var model
            required property string notificationId
            required property bool hasDefaultAction
            required property list<string> actionNames
            required property list<string> actionLabels
            required property bool hasReplyAction
            required property string replyActionLabel
            required property string replyPlaceholderText
            required property string replySubmitButtonText
            required property int urgency
            required property int timeout
            required property list<url> urls
            required property int type
            required property int jobState
            required property string applicationName
            required property string applicationIconName
            required property date updated
            required property string summary
            required property string body
            required property var image
            required property var iconName
            required property int percentage
            required property string jobError
            required property bool suspendable
            required property bool killable
            required property bool resident
            required property string notifyRcName
            required property string desktopEntry

            readonly property bool isJob: type === NotificationManager.Notifications.JobType
            readonly property bool jobRunning: isJob && jobState !== NotificationManager.Notifications.JobStateStopped
            readonly property int behaviour: resident ? NotificationManager.Notifications.None : NotificationManager.Notifications.Close

            function row() {
                return popups.index(index, 0)
            }

            critical: urgency === NotificationManager.Notifications.CriticalUrgency
            takeFocus: toastCard.replying

            onHeightChanged: toasts.place()
            onUpdatedChanged: toastCard.restartTime()

            Component.onCompleted: {
                if (toasts.inhibited)
                    model.wasAddedDuringInhibition = false
                if (!isJob && desktopEntry && !notifyRcName) {
                    settings.registerKnownApplication(desktopEntry)
                    settings.save()
                }
                // the card keeps the time from here on
                popups.stopTimeout(row())
            }

            ToastCard {
                id: toastCard
                applicationName: popup.applicationName
                applicationIconName: popup.applicationIconName
                desktopEntry: popup.desktopEntry
                notifyRcName: popup.notifyRcName
                summary: popup.summary
                body: popup.body
                iconName: popup.iconName || ""
                image: popup.image
                urls: popup.urls
                actionNames: popup.actionNames
                actionLabels: popup.actionLabels
                hasDefaultAction: popup.hasDefaultAction || popup.desktopEntry !== ""
                critical: popup.critical
                isJob: popup.isJob
                killable: !!popup.killable
                suspendable: !!popup.suspendable
                jobState: popup.jobState
                percentage: popup.percentage
                jobError: popup.jobError
                hasReplyAction: popup.hasReplyAction
                replyActionLabel: popup.replyActionLabel
                replyPlaceholderText: popup.replyPlaceholderText
                replySubmitButtonText: popup.replySubmitButtonText
                timeoutMs: Logic.timeoutMs({
                    critical: popup.critical,
                    job: popup.isJob,
                    jobRunning: popup.jobRunning,
                    permanentJobs: settings.permanentJobPopups,
                    seconds: ShellSettings.toastSeconds,
                    timeout: popup.timeout,
                    urlCount: popup.urls.length
                })

                onPointerEntered: popup.model.read = true
                onCloseRequested: {
                    if (popup.jobRunning)
                        popup.model.dismissed = true
                    else
                        popups.close(popup.row())
                }
                onTimeUp: {
                    switch (Logic.whenTimeIsUp({
                        job: popup.isJob,
                        jobRunning: popup.jobRunning,
                        resident: popup.resident,
                        hasActions: popup.hasDefaultAction || popup.actionNames.length > 0 || popup.hasReplyAction,
                        transient: popup.model.transient
                    })) {
                    case "dismiss": popup.model.dismissed = true; break
                    case "hide": popup.model.expired = true; break
                    default: popups.expire(popup.row())
                    }
                }
                onDefaultInvoked: {
                    if (popup.hasDefaultAction)
                        popups.invokeDefaultAction(popup.row(), popup.behaviour)
                    else if (toasts.activateWindowOf(popup.desktopEntry))
                        popups.close(popup.row())
                }
                onActionInvoked: name => popups.invokeAction(popup.row(), name, popup.behaviour)
                onReplied: text => popups.reply(popup.row(), text, popup.behaviour)
                onKillRequested: popups.killJob(popup.row())
                onSuspendRequested: popups.suspendJob(popup.row())
                onResumeRequested: popups.resumeJob(popup.row())
                onThumbnailOpened: url => {
                    Qt.openUrlExternally(url)
                    popups.close(popup.row())
                }
                onMenuRequested: button => menu.openFor(popup, toastCard, button)
                onHeightChanged: toasts.place()
            }
        }
        // ToastStack places the windows on these two as well
        onObjectAdded: (index, object) => {
            if (toasts.centreOpen) {
                // not from inside the model's own change
                Qt.callLater(toasts.hideAll)
                return
            }
            popups.playSoundHint(popups.index(index, 0))
        }
        onObjectRemoved: (index, object) => {
            // the model keeps the time of what no card shows
            popups.startTimeout(object.notificationId)
        }
    }

    // --- the "..." menu ---------------------------------------------------------
    ToastSource { id: words }
    PanelMenu {
        id: menu
        property var popup: null
        property Item card: null
        readonly property string senderName: card ? words.nameOf(card.source) : ""

        function openFor(window, toastCard, button) {
            popup = window
            card = toastCard
            card.menuOpen = true
            open(button, button.width / 2)
        }
        onClosed: if (card) card.menuOpen = false

        RayeMenuItem {
            text: i18nd("raye_shell_clock", "Turn off notifications from %1", menu.senderName)
            icon.name: "notifications-disabled"
            visible: !!menu.popup && (menu.popup.desktopEntry !== "" || menu.popup.notifyRcName !== "")
            onTriggered: {
                Plasmoid.turnOffNotifications(menu.popup.desktopEntry, menu.popup.notifyRcName)
                popups.close(menu.popup.row())
            }
        }
        RayeMenuItem {
            text: i18nd("raye_shell_clock", "Notification settings")
            icon.name: "notifications"
            onTriggered: Launcher.openSettings("system/notifications")
        }
    }
}
