// A plain text link (14 px): "Switch user", "Sign-in options", "OK".
import QtQuick
import QtQuick.Templates as T

T.AbstractButton {
    id: link

    implicitWidth: label.implicitWidth + 8
    implicitHeight: Math.max(28, label.implicitHeight + 8)
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true

    Accessible.role: Accessible.Link
    Accessible.name: text

    Keys.onReturnPressed: clicked()
    Keys.onEnterPressed: clicked()

    contentItem: ShadowText {
        id: label
        text: link.text
        font.pixelSize: Math.round(14 * RayePalette.textScale)
        font.weight: Font.DemiBold
        font.underline: link.hovered
        color: RayePalette.highContrast ? RayePalette.accent : RayePalette.text
        opacity: link.enabled ? (link.pressed ? 0.75 : 1.0) : 0.5
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        radius: RayePalette.controlRadius
        color: "transparent"
        border.width: link.visualFocus ? RayePalette.focusWidth : 0
        border.color: RayePalette.focus
    }
}
