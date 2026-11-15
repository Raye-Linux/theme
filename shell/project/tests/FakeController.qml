// A stand-in for DisplayController: no screens behind it, and what the
// panel asked for in `calls`.
import QtQuick

QtObject {
    property string currentMode: "extend"
    property bool canChoose: true
    property bool ready: true
    property bool busy: false
    property list<string> calls
    signal changed()
    signal activated()
    signal dismissed()
    signal chosen()
    signal failed()
    function choose(mode) {
        calls = Array.from(calls).concat([mode])
        currentMode = mode
        changed()
        chosen()
    }
    function quit() {}
}
