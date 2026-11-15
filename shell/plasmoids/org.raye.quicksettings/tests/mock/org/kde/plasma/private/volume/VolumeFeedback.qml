import QtQuick

// remembers the sinks it was asked to sound on
QtObject {
    property var plays: []
    function play(index) { plays = Array.from(plays).concat([index]) }
}
