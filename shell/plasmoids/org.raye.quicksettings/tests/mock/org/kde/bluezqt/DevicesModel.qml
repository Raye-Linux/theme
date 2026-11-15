// Stand-in for the Bluetooth devices: headphones paired and connected, a
// mouse paired, and a phone in range that was never paired.
import QtQuick

ListModel {
    Component.onCompleted: {
        append({ Name: "Mouse", Icon: "input-mouse", Connected: false, Paired: true, Address: "AA:01", Device: null })
        append({ Name: "Headphones", Icon: "audio-headphones", Connected: true, Paired: true, Address: "AA:02", Device: null })
        append({ Name: "Phone", Icon: "phone", Connected: false, Paired: false, Address: "AA:03", Device: null })
    }
}
