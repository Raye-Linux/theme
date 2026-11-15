// While do not disturb is on, notification sounds stay quiet too. Kept in
// a file of its own: the volume module may be missing.
//
// The sound server can hand over the notification stream after Do not
// disturb was set, or replace it: whenever the stream changes, it is put
// back in line with inhibited.
import QtQuick
import org.kde.plasma.private.volume

QtObject {
    id: sounds

    // set by Toasts.qml: Do not disturb, and the notification settings,
    // which remember that it was us who muted the stream
    property bool inhibited: false
    property QtObject settings: null

    readonly property string streamId: "sink-input-by-media-role:event"
    property QtObject stream

    onInhibitedChanged: apply()
    onSettingsChanged: apply()
    onStreamChanged: apply()

    function apply() {
        if (!stream || !settings)
            return
        if (inhibited) {
            if (!stream.muted) {
                settings.notificationSoundsInhibited = true
                stream.mute()
            }
        } else {
            if (settings.notificationSoundsInhibited)
                stream.unmute()
            settings.notificationSoundsInhibited = false
        }
        settings.save()
    }

    property QtObject instantiator: Instantiator {
        model: StreamRestoreModel {}
        delegate: QtObject {
            readonly property string name: Name
            readonly property bool muted: Muted
            function mute() { Muted = true }
            function unmute() { Muted = false }
        }
        onObjectAdded: (index, object) => {
            if (object.name === sounds.streamId)
                sounds.stream = object
        }
        onObjectRemoved: (index, object) => {
            if (object === sounds.stream)
                sounds.stream = null
        }
    }
}
