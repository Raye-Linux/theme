// The accessibility figure, 16 px, drawn here: the icon themes give the app
// icon for that name, or a disc with the figure cut out, which turns into
// a plain disc once tinted.
import QtQuick

Item {
    id: glyph

    property color color: "white"

    width: 16
    height: 16

    Rectangle {
        x: 6.4
        y: 1
        width: 3.2
        height: 3.2
        radius: 1.6
        color: glyph.color
    }
    // arms
    Rectangle {
        x: 2
        y: 5.2
        width: 12
        height: 1.4
        radius: 0.7
        color: glyph.color
    }
    // body
    Rectangle {
        x: 7.3
        y: 5.6
        width: 1.4
        height: 4.6
        color: glyph.color
    }
    // legs
    Repeater {
        model: [-28, 28]
        Rectangle {
            required property real modelData
            x: 7.3
            y: 9.4
            width: 1.4
            height: 5.6
            radius: 0.7
            color: glyph.color
            transformOrigin: Item.Top
            rotation: modelData
        }
    }
}
