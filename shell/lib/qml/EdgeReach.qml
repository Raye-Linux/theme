// The clicks a taskbar button takes: its own frame plus some room on each
// side, so the bottom edge of the screen, the top of the bar and the gaps
// between buttons are never dead (Fitts). Set it as the button's
// containmentMask; nothing is drawn.
//
//   containmentMask: EdgeReach { target: button; top: 48; bottom: 48; left: 2; right: 2 }
import QtQuick

QtObject {
    property Item target
    property real left: 0
    property real top: 0
    property real right: 0
    property real bottom: 0

    function contains(point: point): bool {
        return !!target && point.x >= -left && point.x < target.width + right
            && point.y >= -top && point.y < target.height + bottom
    }
}
