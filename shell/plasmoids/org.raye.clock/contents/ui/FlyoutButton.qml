// A small button on the notification centre's cards: text or a glyph on
// a backplate that shows on hover (4 px corners), Body type. `accent`
// makes it the filled accent button ("Focus").
import QtQuick
import QtQuick.Templates as T
import org.kde.kirigami as Kirigami
import org.raye.ui
import org.raye.shell

T.AbstractButton {
    id: button

    property bool accent: false
    property string toolTipText: ""

    implicitWidth: Math.max(32, contentItem.implicitWidth + leftPadding + rightPadding)
    implicitHeight: 32
    leftPadding: text !== "" ? 12 : 8
    rightPadding: leftPadding
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.name: toolTipText !== "" ? toolTipText : text

    background: Rectangle {
        radius: ShellTokens.radiusButton
        color: button.accent ? (button.down ? Tokens.accentPressed : button.hovered ? Tokens.accentHover : Tokens.accent)
             : button.down ? ShellTokens.pressFill
             : button.hovered || button.checked ? ShellTokens.hoverFill : "transparent"
        Behavior on color { ColorAnimation { duration: ShellTokens.durationFaster } }
        FocusRing { radius: ShellTokens.radiusButton; target: button }
    }

    contentItem: Item {
        implicitWidth: label.visible ? label.implicitWidth : (icon.visible ? 16 : 0)
        implicitHeight: 20
        Kirigami.Icon {
            id: icon
            anchors.centerIn: parent
            width: 16
            height: 16
            visible: button.text === "" && source !== ""
            source: button.icon.name
            color: button.accent ? ShellTokens.textOnAccent : ShellTokens.text
            isMask: true
        }
        Text {
            id: label
            anchors.centerIn: parent
            visible: button.text !== ""
            text: button.text
            color: button.accent ? ShellTokens.textOnAccent : ShellTokens.text
            font: ShellTokens.body
            opacity: button.enabled ? 1 : 0.5
        }
    }

    RayeTooltip {
        visible: button.hovered && button.toolTipText !== "" && button.toolTipText !== button.text
        text: button.toolTipText
    }
}
