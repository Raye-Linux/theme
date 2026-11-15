// Takes a toast's window to its place in the stack: straight there the
// first time and while it is hidden, with a short slide once it shows.
// No slide when animations are off.
import QtQuick
import org.raye.shell

import "toastlogic.js" as Logic

NumberAnimation {
    id: mover

    property bool placed: false

    property: "y"
    duration: ShellTokens.durationNormal > 0 ? Logic.RESTACK : 0
    easing: Motion.pointToPoint

    function moveTo(x, y) {
        target.x = x
        if (!placed || !target.visible || duration <= 0) {
            stop()
            target.y = y
            placed = true
            return
        }
        if (running ? to === y : target.y === y)
            return
        stop()
        from = target.y
        to = y
        start()
    }
}
