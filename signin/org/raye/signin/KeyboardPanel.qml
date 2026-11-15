// The panel of OnScreenKeyboard.qml, kept apart so a missing
// VirtualKeyboard module only fails this Loader.
import QtQuick
import QtQuick.VirtualKeyboard

Rectangle {
    id: frame

    width: parent ? parent.width : 0
    height: panel.height + 12
    color: RayePalette.highContrast ? "#000000" : RayePalette.nightHigh

    Behavior on y {
        NumberAnimation { duration: Motion.normal; easing.type: Motion.enter }
    }

    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 1
        color: RayePalette.flyoutBorder
    }

    InputPanel {
        id: panel
        y: 12
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(parent.width, 960)
    }
}
