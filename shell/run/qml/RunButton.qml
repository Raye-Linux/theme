// A dialog button: 32 tall, radius 4, accent fill for the default one.
import QtQuick
import QtQuick.Templates as T

T.Button {
    id: button

    property bool accent: false

    implicitWidth: Math.max(96, contentItem.implicitWidth + 2 * Tokens.s5)
    implicitHeight: Tokens.sizeControlHeight
    focusPolicy: Qt.StrongFocus
    Accessible.name: text

    contentItem: Text {
        text: button.text
        font: Tokens.body
        color: !button.enabled ? Tokens.textDisabled
             : button.accent ? Tokens.textOnAccent : Tokens.textPrimary
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Tokens.radiusControl
        color: button.accent
               ? (button.down ? Tokens.accentPressed : button.hovered ? Tokens.accentHover : Tokens.accent)
               : (button.down ? Tokens.fillControlPressed : button.hovered ? Tokens.fillControlHover : Tokens.fillControl)
        border.width: button.accent ? 0 : 1
        border.color: Tokens.strokeControl
        Behavior on color { ColorAnimation { duration: Tokens.durationFaster } }

        // keyboard focus ring: 2 px outer, 1 px inner, outside the button
        Rectangle {
            anchors.fill: parent
            anchors.margins: -3
            radius: Tokens.radiusControl + 3
            color: "transparent"
            border.width: Tokens.sizeFocusOuter
            border.color: Tokens.focusOuter
            visible: button.visualFocus
        }
    }
}
