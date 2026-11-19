// A plain Qt window for titlebar-preview.sh: it says what happens to it.
import QtQuick
import QtQuick.Window

Window {
    width: 640
    height: 400
    visible: true
    title: {
        const a = Qt.application.arguments
        const i = a.indexOf("--")
        return i >= 0 && i + 1 < a.length ? a[i + 1] : "Documents"
    }
    color: "#1a1028"
    onWindowStateChanged: state => console.log("state", state)
    Component.onDestruction: console.log("closed")
    Text {
        anchors.centerIn: parent
        text: "Raye"
        color: "#b9a8d6"
        font.pixelSize: 28
    }
}
