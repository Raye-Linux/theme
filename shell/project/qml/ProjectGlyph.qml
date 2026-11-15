// A small drawing of what a choice does: two screens side by side, lit or
// dark. Drawn here so it follows the accent, and it is Raye's own.
import QtQuick
import org.raye.shell

Item {
    id: glyph

    // "pc-only", "duplicate", "extend", "second-only"
    property string mode: ""
    property color lit: ShellTokens.accent
    property color frame: ShellTokens.textSecondary

    implicitWidth: 44
    implicitHeight: 28

    component Screen: Rectangle {
        property bool on: false
        width: 20
        height: 16
        y: 6
        radius: 2
        color: on ? glyph.lit : "transparent"
        border.width: 1
        border.color: on ? glyph.lit : glyph.frame
    }

    Screen {
        id: first
        x: 0
        on: glyph.mode !== "second-only"
    }
    Screen {
        id: second
        x: 24
        on: glyph.mode !== "pc-only"
    }
    // Duplicate: the same mark on both
    Repeater {
        model: glyph.mode === "duplicate" ? [first, second] : []
        Text {
            required property Item modelData
            x: modelData.x
            y: modelData.y
            width: modelData.width
            height: modelData.height
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: "1"
            font.pixelSize: 10
            font.weight: Font.DemiBold
            color: ShellTokens.textOnAccent
        }
    }
    // Extend: one desktop across the two
    Rectangle {
        visible: glyph.mode === "extend"
        x: 18
        y: 13
        width: 8
        height: 2
        color: ShellTokens.textOnAccent
    }
}
