// Stand-in for the sound server's saved streams in the ToastSounds test.
// The notification stream shows up a moment after the list, the way it
// does when the sound server answers after the clock has loaded.
import QtQuick

ListModel {
    id: model

    // the test reads this to set the stream's state before it appears
    property bool eventMuted: false

    Component.onCompleted: append({ Name: "sink-input-by-media-role:music", Muted: false })

    property Timer late: Timer {
        interval: 100
        running: true
        onTriggered: model.append({ Name: "sink-input-by-media-role:event", Muted: model.eventMuted })
    }
}
