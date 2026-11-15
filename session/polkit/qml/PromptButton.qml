// Yes and No: 200 x 32, radius 4; Yes is filled with the accent.
import QtQuick
import QtQuick.Templates as T
import org.raye.signin

T.Button {
    id: button

    property bool accent: false
    // PAM is checking: a spinner in place of the word
    property bool busy: false

    implicitWidth: 200
    implicitHeight: 32
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true

    Accessible.role: Accessible.Button
    Accessible.name: text

    Keys.onSpacePressed: clicked()

    contentItem: Text {
        text: button.busy ? "" : button.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        color: button.accent ? RayePalette.textOnAccent : RayePalette.text
        opacity: button.enabled ? 1 : 0.5
        font.family: RayePalette.family
        font.pixelSize: Math.round(14 * RayePalette.textScale)
        font.weight: button.accent ? Font.DemiBold : Font.Normal
    }

    Spinner {
        anchors.centerIn: parent
        size: 18
        visible: button.busy
    }

    background: Rectangle {
        radius: RayePalette.controlRadius
        // busy: a quiet fill, so the violet spinner shows
        color: button.busy ? "#1af1f0f4"
               : button.accent
               ? (button.pressed ? RayePalette.accentPressed
                                 : button.hovered ? RayePalette.accentHover : RayePalette.accent)
               : (button.pressed ? RayePalette.pressFill
                                 : button.hovered ? "#24f1f0f4" : "#1af1f0f4")
        opacity: button.enabled || button.busy ? 1 : 0.5
        border.width: button.visualFocus ? RayePalette.focusWidth : (button.accent ? 0 : 1)
        border.color: button.visualFocus ? RayePalette.focus : "#14f1f0f4"
    }
}
