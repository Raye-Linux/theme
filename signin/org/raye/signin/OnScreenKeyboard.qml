// The on-screen keyboard: QtQuick.VirtualKeyboard's panel in a Raye
// frame, sliding up from the bottom edge of its parent while `active`.
// Typing reaches the focused field when the greeter runs with
// QT_IM_MODULE=qtvirtualkeyboard.
//
// The panel is only loaded when first shown, and the rest of the screen
// works when the VirtualKeyboard module is missing: `available` turns
// false and the accessibility flyout hides the row.
import QtQuick

Item {
    id: keyboard

    property bool active: false
    readonly property bool available: loader.status !== Loader.Error
    readonly property real panelHeight: loader.item ? loader.item.height : 0
    // the space it takes at the bottom of the screen right now
    readonly property real occupied: loader.item ? Math.max(0, height - loader.item.y) : 0

    anchors.left: parent ? parent.left : undefined
    anchors.right: parent ? parent.right : undefined
    anchors.bottom: parent ? parent.bottom : undefined
    height: parent ? parent.height : 0
    clip: true
    visible: occupied > 0

    property bool _wanted: false
    onActiveChanged: {
        if (active)
            _wanted = true
        if (active)
            Qt.inputMethod.show()
        else
            Qt.inputMethod.hide()
    }

    Loader {
        id: loader
        width: parent.width
        active: keyboard._wanted
        source: "KeyboardPanel.qml"
        onLoaded: {
            // start below the edge so the first show slides in too
            item.y = keyboard.height
            item.y = Qt.binding(function () {
                return keyboard.active ? keyboard.height - item.height : keyboard.height
            })
        }
    }
}
