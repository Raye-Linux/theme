// Sizes and places the window of the notification centre. The centre is
// laid out for the room of the clock's screen before the window shows, and
// the window is given its size here: a Plasma dialog does not follow its
// main item while hidden, and once resized it stops following it at all, so
// a centre last opened on a taller screen came up at the old size.
import QtQuick

import "clocklogic.js" as Logic

QtObject {
    property var window
    // the window's main item, the centre's parent
    property Item holder
    property Item centre
    // from the screen's right edge and from the taskbar
    readonly property int edge: 12

    function open(screenRect, panelTop, area) {
        centre.prepare(Logic.centreRoom(screenRect, panelTop, area, edge))
        fit(screenRect, panelTop)
    }

    // the centre changed height while open (a notification came or went)
    function fit(screenRect, panelTop) {
        const g = Logic.centreGeometry(screenRect, panelTop, centre.width, centre.height, centre.margin, edge)
        holder.width = g.width
        holder.height = g.height
        window.width = g.width
        window.height = g.height
        window.x = g.x
        window.y = g.y
    }
}
