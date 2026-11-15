// The search glass, drawn: a ring and its handle, 16 px. It tilts a
// little when the pointer is over it, the way Windows' glass does.
import QtQuick

Canvas {
    id: glyph

    property color color: "white"
    property bool tilted: false

    implicitWidth: 16
    implicitHeight: 16
    rotation: tilted ? -12 : 0
    Behavior on rotation { NumberAnimation { duration: 167; easing.type: Easing.OutCubic } }
    onColorChanged: requestPaint()

    onPaint: {
        const ctx = getContext("2d")
        const s = Math.min(width, height) / 16
        ctx.reset()
        ctx.strokeStyle = glyph.color
        ctx.lineWidth = 1.6 * s
        ctx.lineCap = "round"
        ctx.beginPath()
        ctx.arc(6.75 * s, 6.75 * s, 4.75 * s, 0, 2 * Math.PI)
        ctx.stroke()
        ctx.beginPath()
        ctx.moveTo(10.4 * s, 10.4 * s)
        ctx.lineTo(14.2 * s, 14.2 * s)
        ctx.stroke()
    }
}
