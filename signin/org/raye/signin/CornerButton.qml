// A square icon button: the corner bar buttons (40 x 40, 20 px icon), and
// the eye and arrow of the password field.
//
// iconName is a Raye sign-in icon (RayePalette.icon); text replaces the icon
// (the keyboard layout code "FR"). accentHover fills it with the accent on
// hover, like the submit arrow.
import QtQuick
import QtQuick.Templates as T

T.AbstractButton {
    id: button

    property string iconName: ""
    property int size: 40
    property int iconSize: 20
    property bool accentHover: false
    property int radius: RayePalette.controlRadius
    property string accessibleName: text

    implicitWidth: Math.max(size, label.visible ? label.implicitWidth + 16 : 0)
    implicitHeight: size
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true

    Accessible.role: Accessible.Button
    Accessible.name: accessibleName

    Keys.onReturnPressed: clicked()
    Keys.onEnterPressed: clicked()

    contentItem: Item {
        Image {
            anchors.centerIn: parent
            visible: button.iconName !== ""
            source: button.iconName !== "" ? RayePalette.icon(button.iconName) : ""
            sourceSize: Qt.size(button.iconSize, button.iconSize)
            width: button.iconSize
            height: button.iconSize
            opacity: button.enabled ? 1.0 : 0.4
        }
        ShadowText {
            id: label
            anchors.centerIn: parent
            visible: button.iconName === "" && button.text !== ""
            text: button.text
            shadow: false
            font.pixelSize: Math.round(14 * RayePalette.textScale)
            font.weight: Font.DemiBold
            opacity: button.enabled ? 1.0 : 0.4
        }
    }

    background: Rectangle {
        radius: button.radius
        color: {
            if (button.accentHover && button.enabled && (button.hovered || button.pressed))
                return button.pressed ? RayePalette.accentPressed : RayePalette.accent
            if (button.pressed)
                return RayePalette.pressFill
            if (button.hovered || button.checked)
                return RayePalette.hoverFill
            return "transparent"
        }
        border.width: button.visualFocus ? RayePalette.focusWidth : 0
        border.color: RayePalette.focus

        Behavior on color {
            ColorAnimation { duration: Motion.fast }
        }
    }
}
