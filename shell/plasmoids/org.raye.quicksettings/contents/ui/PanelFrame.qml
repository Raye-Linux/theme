// The item of the panel's window. Wayland places a popup once, when it
// maps: if the window grows later it grows downwards, under the taskbar,
// and moving it again comes a frame late. So the window is made as tall as
// the tallest page and keeps that size; the panel sits at its bottom.
import QtQuick

FocusScope {
    id: frame

    // what the window wraps
    property Item panel: null
    readonly property int panelHeight: panel ? Math.max(1, Math.ceil(panel.height)) : 1
    readonly property int reserve: panel ? Math.ceil(panel.reserveHeight) : 1
    // the empty room above the panel
    signal outsideClicked()

    width: panel ? Math.max(1, Math.ceil(panel.width)) : 1
    height: Math.max(panelHeight, reserve)

    MouseArea {
        anchors.fill: parent
        onClicked: frame.outsideClicked()
    }
}
