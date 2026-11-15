// Where the toast windows go: the newest just above the taskbar, each
// older one above it, at the right edge of the taskbar's screen. When one
// goes, the others slide down into the room it leaves; when one comes,
// the others slide up.
import QtQuick
import org.raye.shell

import "toastlogic.js" as Logic

Item {
    id: stack

    // the Instantiator of the toast windows; each one has moveTo(x, y)
    property var windows: null
    // the screen of the taskbar and the top of the taskbar on it
    property rect area
    property int panelTop: 0
    property bool hidden: false

    visible: false

    onAreaChanged: place()
    onPanelTopChanged: place()
    onHiddenChanged: place()

    function list(leaving) {
        const out = []
        if (!windows)
            return out
        for (let i = 0; i < windows.count; ++i) {
            const w = windows.objectAt(i)
            if (w && w !== leaving)
                out.push(w)
        }
        return out
    }

    function place(leaving) {
        const all = list(leaving)
        if (all.length === 0 || area.width <= 0)
            return
        const top = panelTop > area.y ? panelTop : area.y + area.height - ShellTokens.taskbarHeight
        const margin = 8
        const bottom = top - Logic.EDGE
        const room = (bottom - area.y) * 0.8
        const spots = Logic.stack(all.map(w => w.height), bottom, margin, Logic.GAP, room)
        for (let i = 0; i < all.length; ++i) {
            const w = all[i]
            // moved first: a window still hidden goes straight to its place
            w.moveTo(area.x + area.width - w.width, spots[i].y)
            w.visible = spots[i].visible && !hidden
        }
    }

    Connections {
        target: stack.windows
        function onObjectAdded(index, object) {
            stack.place()
        }
        // The Instantiator still lists the window it is removing while it
        // says so: placing the others now would keep it as the newest and
        // leave the one that stays floating where it was.
        function onObjectRemoved(index, object) {
            stack.place(object)
            Qt.callLater(stack.place)
        }
    }
}
