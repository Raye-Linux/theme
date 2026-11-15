// What every screen's copy of Logout.qml has to agree on. The greeter makes
// one window per screen on one QML engine, so this is the one object they
// all see: a choice made on one screen shows on all of them, and the
// session is only asked once.
pragma Singleton

import QtQuick

QtObject {
    // "", or the StatusScreen mode the session is ending under
    property string phase: ""
    property bool asked: false

    // true for the first caller only
    function claim() {
        if (asked)
            return false
        asked = true
        return true
    }

    function reset() {
        phase = ""
        asked = false
    }
}
