// The upper card of the notification centre: "Notifications", "Clear
// all" and the Do not disturb bell, then the notifications grouped by app,
// or "No new notifications".
import QtQuick
import org.raye.ui
import org.raye.shell

Item {
    id: card

    property var model
    property var controller
    property date now: new Date()
    property bool doNotDisturb: false
    // the card never grows past this
    property int maxHeight: 600
    readonly property int count: list.count
    // the header and a line under it, however small the screen
    readonly property int minimumHeight: header.height + 72

    implicitWidth: 364
    // as tall as what it holds, up to maxHeight; then the list scrolls
    readonly property int naturalHeight: header.height + (count > 0 ? list.contentHeight + 12 : empty.height)
    implicitHeight: Math.min(maxHeight, naturalHeight)

    // the list lays itself out once per frame: measure it now
    function layoutNow() {
        list.forceLayout()
    }

    AcrylicSurface {
        anchors.fill: parent
        focused: card.activeFocus
    }

    Item {
        id: header
        width: parent.width
        height: 52

        Text {
            anchors { left: parent.left; leftMargin: 16; verticalCenter: parent.verticalCenter }
            text: i18nd("raye_shell_clock", "Notifications")
            color: ShellTokens.text
            font: ShellTokens.bodyStrong
        }
        Row {
            anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
            spacing: 4
            FlyoutButton {
                objectName: "clearAll"
                visible: card.count > 0
                text: i18nd("raye_shell_clock", "Clear all")
                onClicked: card.controller.clearAll()
            }
            FlyoutButton {
                id: dndButton
                objectName: "dndButton"
                checkable: true
                checked: card.doNotDisturb
                toolTipText: card.doNotDisturb ? i18nd("raye_shell_clock", "Turn off do not disturb")
                                               : i18nd("raye_shell_clock", "Turn on do not disturb")
                Accessible.role: Accessible.CheckBox
                onClicked: card.controller.toggleDnd()

                BellGlyph {
                    anchors.centerIn: parent
                    width: 16
                    height: 16
                    color: card.doNotDisturb ? ShellTokens.accent : ShellTokens.text
                    sleeping: card.doNotDisturb
                }
            }
        }
    }

    ListView {
        id: list
        objectName: "notificationList"
        anchors { left: parent.left; right: parent.right; top: header.bottom; bottom: parent.bottom
                  leftMargin: 12; rightMargin: 12; bottomMargin: 12 }
        clip: true
        model: card.model
        boundsBehavior: Flickable.StopAtBounds
        spacing: 0
        delegate: NotificationItem {
            controller: card.controller
            now: card.now
        }
        add: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: ShellTokens.durationNormal; easing: Motion.entrance }
        }
        displaced: Transition {
            NumberAnimation { property: "y"; duration: ShellTokens.durationNormal; easing: Motion.pointToPoint }
        }
    }

    Text {
        id: empty
        objectName: "empty"
        visible: card.count === 0
        anchors { top: header.bottom; horizontalCenter: parent.horizontalCenter }
        height: 44
        text: i18nd("raye_shell_clock", "No new notifications")
        color: ShellTokens.textSecondary
        font: ShellTokens.body
        verticalAlignment: Text.AlignTop
    }
}
