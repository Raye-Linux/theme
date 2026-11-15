// The Start applets of the session (one per taskbar, one taskbar per
// screen) share this file: Plasma loads every applet in one QML engine and
// a library script once per engine. A global shortcut is a single action
// of plasmashell, so whichever Start receives it hands it to the Start on
// the screen in use, like the lone Win key, or else to the one the person
// used last (a click uses the one clicked).
.pragma library

var starts = []
var last = null
var handledAt = {}

function add(start) {
    if (starts.indexOf(start) < 0)
        starts.push(start)
    if (!last)
        last = start
}

function remove(start) {
    starts = starts.filter(function (s) { return s !== start })
    if (last === start)
        last = starts.length > 0 ? starts[0] : null
}

function used(start) {
    last = start
}

// The Start a shortcut goes to; null when the same shortcut was just
// handled (more than one Start received it). `area` is the screen in use
// (KWin's active output, where the pointer is): the Start on that screen
// gets it, so the menu never opens out of sight on another screen. Without
// one there, the Start used last.
function target(name, now, area) {
    if (handledAt[name] !== undefined && now - handledAt[name] < 150)
        return null
    handledAt[name] = now
    var here = onScreen(area)
    return here ? here : last
}

function onScreen(area) {
    if (!area || !(area.width > 0) || !(area.height > 0))
        return null
    var cx = area.x + area.width / 2
    var cy = area.y + area.height / 2
    // the one used last first, when two Starts share a screen
    var order = last ? [last].concat(starts) : starts
    for (var i = 0; i < order.length; ++i) {
        var r = order[i] && order[i].screenRect
        if (r && cx >= r.x && cx < r.x + r.width && cy >= r.y && cy < r.y + r.height)
            return order[i]
    }
    return null
}
