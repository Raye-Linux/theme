// Once a tooltip has shown, the next one comes at once for a second, so
// moving along the taskbar does not wait half a second at every button.
pragma Singleton

import QtQuick

QtObject {
    id: warmth

    property bool warm: false
    readonly property Timer cooling: Timer {
        interval: 1000
        onTriggered: warmth.warm = false
    }

    function shown() {
        cooling.stop()
        warm = true
    }

    function hidden() {
        cooling.restart()
    }
}
