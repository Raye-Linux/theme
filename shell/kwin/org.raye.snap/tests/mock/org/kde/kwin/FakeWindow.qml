// A window for the snap tests: what the script reads and writes.
import QtQuick

QtObject {
    property string caption: "Window"
    property var internalId: Math.random().toString(36).slice(2)
    property rect frameGeometry
    property var output: null
    property int maximizeMode: 0
    property var tile: null
    property bool normalWindow: true
    property bool fullScreen: false
    property bool minimized: false
    property bool minimizable: true
    property bool maximizable: true
    property bool moveable: true
    property bool resizeable: true
    property bool onAllDesktops: false
    property var desktops: [1]
    property bool move: false
    property bool resize: false
    signal interactiveMoveResizeStarted()
    signal interactiveMoveResizeStepped(rect geometry)
    signal interactiveMoveResizeFinished()
    signal frameGeometryAboutToChange()

    function setMaximize(vertically, horizontally) {
        maximizeMode = (vertically ? 1 : 0) | (horizontally ? 2 : 0);
    }
}
