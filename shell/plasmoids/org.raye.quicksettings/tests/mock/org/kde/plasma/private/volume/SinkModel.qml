// Stand-in for the sound outputs: speakers in use, and headphones.
import QtQuick

ListModel {
    Component.onCompleted: {
        append({ Name: "speakers", Description: "Speakers", Default: true, PulseObject: null })
        append({ Name: "auto_null", Description: "Dummy Output", Default: false, PulseObject: null })
        append({ Name: "jack", Description: "Headphones", Default: false, PulseObject: null })
    }
}
