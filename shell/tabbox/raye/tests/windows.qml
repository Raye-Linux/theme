// The windows the tests and the harness show: a ListModel with KWin's
// roles and its two calls. "1600x900" in a window id is its size.
import QtQuick

ListModel {
    id: windows
    property var closed: []
    property int activated: -1

    function close(index) {
        closed = closed.concat([index])
        remove(index)
    }
    function add(w) {
        append(w)
    }
    function activate(index) {
        activated = index
    }
}
