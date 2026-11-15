// The clock on the taskbar: time over the short date, right aligned in
// Caption type with figures of even width, and the bell after them (with
// the unread count in an accent circle, or a "z" when Do not disturb is
// on). One hover target, as in Windows. While a Focus session runs a
// thin ring around the bell shows what is left of it.
import QtQuick
import QtQuick.Templates as T
import org.raye.ui
import org.raye.shell

import "clocklogic.js" as Logic

T.AbstractButton {
    id: clock

    property date now: new Date()
    property bool twentyFour: true
    property bool showSeconds: false
    property bool showDate: true
    property int unreadCount: 0
    property bool doNotDisturb: false
    // 0 to 1 while a Focus session runs, -1 otherwise
    property real focusFraction: -1
    property bool selected: false
    property string longDate: ""

    readonly property string timeText: Logic.timeText(now.getHours(), now.getMinutes(), now.getSeconds(), twentyFour, showSeconds)
    readonly property string dateText: now.toLocaleDateString(Qt.locale(), Locale.ShortFormat)
    readonly property string countText: Logic.countText(unreadCount)

    implicitWidth: row.implicitWidth + 2 * 8
    implicitHeight: ShellTokens.buttonSize
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.role: Accessible.Button
    Accessible.name: longDate + " " + timeText
    Accessible.description: unreadCount > 0 ? i18ndp("raye_shell_clock", "%1 new notification", "%1 new notifications", unreadCount)
                          : doNotDisturb ? i18nd("raye_shell_clock", "Do not disturb is on") : ""

    background: Rectangle {
        radius: ShellTokens.radiusButton
        color: clock.down ? ShellTokens.taskbarPressFill
             : clock.selected ? ShellTokens.taskbarSelectedFill
             : clock.hovered ? ShellTokens.taskbarHoverFill : "transparent"
        Behavior on color { ColorAnimation { duration: ShellTokens.durationFaster } }
        FocusRing { radius: ShellTokens.radiusButton; target: clock }
    }

    contentItem: Item {
        Row {
            id: row
            anchors.centerIn: parent
            spacing: 8

            Column {
                anchors.verticalCenter: parent.verticalCenter
                // room between the two lines, and it gives way before
                // the stack outgrows the button: see Logic.lineGap
                spacing: Logic.lineGap(timeLabel.implicitHeight,
                                       dateLabel.visible ? dateLabel.implicitHeight : 0,
                                       clock.height > 0 ? clock.height : clock.implicitHeight)
                Text {
                    id: timeLabel
                    objectName: "time"
                    anchors.right: parent.right
                    text: clock.timeText
                    color: ShellTokens.taskbarText
                    font.family: "Inter"
                    font.pixelSize: 12
                    font.features: { "tnum": 1 }
                }
                Text {
                    id: dateLabel
                    objectName: "date"
                    anchors.right: parent.right
                    visible: clock.showDate
                    text: clock.dateText
                    color: ShellTokens.taskbarText
                    font.family: "Inter"
                    font.pixelSize: 12
                    font.features: { "tnum": 1 }
                }
            }

            // the bell, its count or its "z", and the Focus ring around it
            Item {
                id: bellHolder
                anchors.verticalCenter: parent.verticalCenter
                width: 20
                height: 20

                Canvas {
                    id: ring
                    objectName: "focusRing"
                    anchors.fill: parent
                    visible: clock.focusFraction >= 0
                    property real fraction: Math.max(0, clock.focusFraction)
                    onFractionChanged: requestPaint()
                    onVisibleChanged: requestPaint()
                    onPaint: {
                        const ctx = getContext("2d")
                        ctx.reset()
                        ctx.lineWidth = 1.5
                        ctx.strokeStyle = Qt.alpha(ShellTokens.taskbarText, 0.2)
                        ctx.beginPath()
                        ctx.arc(width / 2, height / 2, width / 2 - 1, 0, 2 * Math.PI)
                        ctx.stroke()
                        ctx.strokeStyle = ShellTokens.accent
                        ctx.lineCap = "round"
                        ctx.beginPath()
                        ctx.arc(width / 2, height / 2, width / 2 - 1, -Math.PI / 2, -Math.PI / 2 + 2 * Math.PI * fraction)
                        ctx.stroke()
                    }
                }

                BellGlyph {
                    objectName: "bell"
                    anchors.centerIn: parent
                    width: 16
                    height: 16
                    visible: clock.countText === ""
                    color: ShellTokens.taskbarText
                    sleeping: clock.doNotDisturb
                    scale: clock.focusFraction >= 0 ? 0.8 : 1
                }

                Rectangle {
                    objectName: "count"
                    anchors.centerIn: parent
                    visible: clock.countText !== ""
                    width: Math.max(16, countLabel.implicitWidth + 6)
                    height: 16
                    radius: 8
                    color: ShellTokens.accent
                    Text {
                        id: countLabel
                        anchors.centerIn: parent
                        text: clock.countText
                        color: ShellTokens.textOnAccent
                        font.family: "Inter"
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                        font.features: { "tnum": 1 }
                    }
                }
            }
        }
    }

    RayeTooltip {
        visible: clock.hovered && !clock.down && !clock.selected && clock.longDate !== ""
        text: clock.longDate
    }
}
