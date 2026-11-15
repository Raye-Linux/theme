// Stand-in for KWin's TabBoxSwitcher in the tests and the harness: a
// plain object (not an item) with the properties the switcher QML reads.
import QtQuick

QtObject {
    default property list<QtObject> data
    property bool visible: false
    property var model: null
    property int currentIndex: 0
    property rect screenGeometry: Qt.rect(0, 0, 1920, 1080)
    property bool noModifierGrab: false
}
