// A line above the password field for another way in, e.g. "Scan your
// finger on the fingerprint reader". It shows only while `text` is set;
// the lock screen decides when (only after the reader has spoken).
import QtQuick

Item {
    id: hint

    property string text: ""
    property string iconName: ""

    implicitWidth: row.implicitWidth
    implicitHeight: text !== "" ? row.implicitHeight : 0
    opacity: text !== "" ? 1 : 0
    visible: opacity > 0

    Behavior on opacity {
        NumberAnimation { duration: Motion.normal; easing.type: Motion.enter }
    }

    Accessible.role: Accessible.StaticText
    Accessible.name: text

    Row {
        id: row
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 8

        Image {
            visible: hint.iconName !== ""
            anchors.verticalCenter: parent.verticalCenter
            source: hint.iconName !== "" ? RayePalette.icon(hint.iconName) : ""
            sourceSize: Qt.size(16, 16)
            width: 16
            height: 16
        }

        ShadowText {
            anchors.verticalCenter: parent.verticalCenter
            text: hint.text
            font.pixelSize: Math.round(14 * RayePalette.textScale)
            color: RayePalette.text
        }
    }
}
