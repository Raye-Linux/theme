// One row of the notifications card: either the header of an app's group
// (its icon, its name and "x" to clear them all) or a notification (title,
// text, picture, buttons, how old it is) or a job, such as a file copy,
// with its progress. Plain text only: what an app sends is never read as
// markup, so it cannot load anything.
import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.kquickcontrolsaddons as KQuickAddons
import org.raye.ui
import org.raye.shell

import "clocklogic.js" as Logic
import "toasts"
import "toasts/toastlogic.js" as ToastLogic

Item {
    id: item

    required property int index
    required property var model
    property var controller
    property date now: new Date()
    property int jobType: 2          // NotificationManager.Notifications.JobType
    property int jobRunning: 1       // .JobStateRunning
    property int jobSuspended: 2     // .JobStateSuspended

    readonly property bool isGroup: !!model.isGroup
    readonly property bool isJob: model.type === jobType
    readonly property bool inGroup: !!model.isInGroup
    readonly property var actionNames: list(model.actionNames)
    readonly property var actionLabels: list(model.actionLabels)

    width: ListView.view ? ListView.view.width : 320
    height: isGroup ? groupRow.height : card.height + 8

    // a list role: a string list from Plasma (a list model, as in the
    // tests, can only keep it as lines of text)
    function list(value) {
        if (value === undefined || value === null)
            return []
        if (typeof value === "string")
            return value === "" ? [] : value.split("\n")
        return value
    }

    ToastSource { id: words }
    readonly property var source: ToastLogic.source(model.applicationName, model.applicationIconName, model.desktopEntry, model.notifyRcName, isJob)
    readonly property var networkEvent: ToastLogic.networkEvent(model.notifyRcName, model.body)

    function plain(text) {
        return ToastLogic.plain(text)
    }

    function ageText(created) {
        const a = Logic.age(created, item.now)
        switch (a.kind) {
        case "now": return i18nd("raye_shell_clock", "Now")
        case "minutes": return i18ndc("raye_shell_clock", "minutes ago, short", "%1m", a.value)
        case "today": return Qt.formatTime(created, Logic.timeFormat(Logic.use24h("auto", Qt.locale().timeFormat(Locale.ShortFormat)), false))
        case "yesterday": return i18nd("raye_shell_clock", "Yesterday")
        default: return created.toLocaleDateString(Qt.locale(), Locale.ShortFormat)
        }
    }

    // --- the header of an app's group --------------------------------------
    Item {
        id: groupRow
        visible: item.isGroup
        width: parent.width
        height: 36

        Kirigami.Icon {
            id: appIcon
            anchors { left: parent.left; leftMargin: 4; verticalCenter: parent.verticalCenter }
            width: 16
            height: 16
            source: item.source.icon
        }
        Text {
            anchors { left: appIcon.right; leftMargin: 8; right: clearGroup.left; rightMargin: 8; verticalCenter: parent.verticalCenter }
            text: words.nameOf(item.source)
            elide: Text.ElideRight
            color: ShellTokens.text
            font: ShellTokens.caption
            textFormat: Text.PlainText
        }
        FlyoutButton {
            id: clearGroup
            anchors { right: parent.right; verticalCenter: parent.verticalCenter }
            icon.name: "window-close-symbolic"
            toolTipText: i18nd("raye_shell_clock", "Clear these notifications")
            onClicked: item.controller.close(item.index)
        }
    }

    // --- a notification or a job ---------------------------------------------
    Rectangle {
        id: card
        visible: !item.isGroup
        width: parent.width
        height: body.implicitHeight + 24
        radius: 6
        color: hover.hovered ? Tokens.bgCardHover : Tokens.bgCard
        border.width: 1
        border.color: Tokens.strokeCard

        HoverHandler { id: hover }
        TapHandler {
            enabled: !!item.model.hasDefaultAction
            onTapped: item.controller.invokeDefault(item.index)
        }

        // how old it is and the "x", at the top right of the card
        Row {
            id: corner
            z: 1
            anchors { right: parent.right; rightMargin: 4; top: parent.top; topMargin: 8 }
            spacing: 2
            Text {
                objectName: "age"
                anchors.verticalCenter: parent.verticalCenter
                visible: !item.isJob
                text: item.model.created ? item.ageText(item.model.created) : ""
                color: ShellTokens.textSecondary
                font: ShellTokens.caption
            }
            FlyoutButton {
                id: closeButton
                width: 28
                height: 28
                icon.name: "window-close-symbolic"
                toolTipText: item.isJob ? i18nd("raye_shell_clock", "Cancel") : i18nd("raye_shell_clock", "Clear")
                visible: item.isJob ? !!item.model.killable && item.model.jobState !== 0 : true
                opacity: hover.hovered || item.isJob || activeFocus ? 1 : 0
                onClicked: item.isJob ? item.controller.killJob(item.index) : item.controller.close(item.index)
            }
        }

        Column {
            id: body
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 12 }
            spacing: 4

            // the app's name, when the notification is not inside a group
            Row {
                visible: !item.inGroup
                height: 20
                spacing: 8
                Kirigami.Icon {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 16
                    height: 16
                    source: item.source.icon
                }
                Text {
                    objectName: "appName"
                    anchors.verticalCenter: parent.verticalCenter
                    text: words.nameOf(item.source)
                    color: ShellTokens.textSecondary
                    font: ShellTokens.caption
                    textFormat: Text.PlainText
                }
            }

            Text {
                objectName: "summary"
                width: parent.width - (item.inGroup ? corner.width + 4 : 0)
                text: item.networkEvent ? words.networkTitle(item.networkEvent) : ToastLogic.title(item.model.summary)
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
                color: ShellTokens.text
                font: ShellTokens.bodyStrong
                textFormat: Text.PlainText
                visible: text !== ""
            }
            Text {
                objectName: "body"
                width: parent.width
                text: item.networkEvent ? "" : item.plain(item.model.body)
                wrapMode: Text.Wrap
                maximumLineCount: 3
                elide: Text.ElideRight
                color: ShellTokens.textSecondary
                font: ShellTokens.body
                textFormat: Text.PlainText
                visible: text !== "" && !item.isJob
            }

            // a job's progress: a bar 4 px tall and the percentage
            Item {
                visible: item.isJob
                width: parent.width
                height: 24
                Rectangle {
                    id: track
                    anchors { left: parent.left; right: percent.left; rightMargin: 8; verticalCenter: parent.verticalCenter }
                    height: 4
                    radius: 2
                    color: Qt.alpha(ShellTokens.text, 0.15)
                    Rectangle {
                        height: parent.height
                        radius: 2
                        width: parent.width * Math.max(0, Math.min(100, item.model.percentage || 0)) / 100
                        color: item.model.jobState === item.jobSuspended ? Tokens.statusCaution : ShellTokens.accent
                    }
                }
                Text {
                    id: percent
                    anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                    text: i18ndc("raye_shell_clock", "progress of a file copy", "%1%", Math.round(item.model.percentage || 0))
                    color: ShellTokens.textSecondary
                    font.family: "Inter"
                    font.pixelSize: 12
                    font.features: { "tnum": 1 }
                }
            }

            // the picture an app sent with the notification
            KQuickAddons.QImageItem {
                id: picture
                visible: !item.isJob && nativeWidth > 0
                width: parent.width
                height: visible ? Math.min(160, Math.round(width * nativeHeight / Math.max(1, nativeWidth))) : 0
                fillMode: KQuickAddons.QImageItem.PreserveAspectCrop
                smooth: true
                Binding on image {
                    when: item.model.image !== undefined && item.model.image !== null
                    value: item.model.image
                }
            }

            // the app's buttons, side by side, the full width
            Row {
                visible: item.actionNames.length > 0 && !item.isJob
                width: parent.width
                spacing: 8
                topPadding: 4
                Repeater {
                    model: Math.min(3, item.actionNames.length)
                    FlyoutButton {
                        required property int index
                        width: (parent.width - (Math.min(3, item.actionNames.length) - 1) * 8) / Math.min(3, item.actionNames.length)
                        text: item.actionLabels[index] || ""
                        background: Rectangle {
                            radius: ShellTokens.radiusButton
                            color: parent.down ? Tokens.fillControlPressed : parent.hovered ? Tokens.fillControlHover : Tokens.fillControl
                            border.width: 1
                            border.color: Tokens.strokeControl
                        }
                        onClicked: item.controller.invokeAction(item.index, item.actionNames[index])
                    }
                }
            }
        }
    }
}
