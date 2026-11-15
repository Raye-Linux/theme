// The notification bell, drawn: a bell and its clapper, 16 px. Asleep
// (Do not disturb) it carries a small "z" at the top right.
import QtQuick

Canvas {
    id: glyph

    property color color: "white"
    property bool sleeping: false

    implicitWidth: 16
    implicitHeight: 16
    onColorChanged: requestPaint()
    onSleepingChanged: requestPaint()

    onPaint: {
        const ctx = getContext("2d")
        const s = Math.min(width, height) / 16
        ctx.reset()
        ctx.strokeStyle = glyph.color
        ctx.fillStyle = glyph.color
        ctx.lineWidth = 1.3 * s
        ctx.lineJoin = "round"
        ctx.lineCap = "round"
        // the bell
        ctx.beginPath()
        ctx.moveTo(3 * s, 11.5 * s)
        ctx.lineTo(4.2 * s, 10 * s)
        ctx.lineTo(4.2 * s, 7 * s)
        ctx.bezierCurveTo(4.2 * s, 4.4 * s, 5.9 * s, 2.6 * s, 8 * s, 2.6 * s)
        ctx.bezierCurveTo(10.1 * s, 2.6 * s, 11.8 * s, 4.4 * s, 11.8 * s, 7 * s)
        ctx.lineTo(11.8 * s, 10 * s)
        ctx.lineTo(13 * s, 11.5 * s)
        ctx.closePath()
        ctx.stroke()
        // the clapper
        ctx.beginPath()
        ctx.moveTo(6.6 * s, 13.4 * s)
        ctx.quadraticCurveTo(8 * s, 14.8 * s, 9.4 * s, 13.4 * s)
        ctx.stroke()
        if (sleeping) {
            // a "z", over a cut in the bell's shoulder
            ctx.clearRect(9.6 * s, 0, 6.4 * s, 6.4 * s)
            ctx.lineWidth = 1.2 * s
            ctx.beginPath()
            ctx.moveTo(10.6 * s, 1.2 * s)
            ctx.lineTo(14.6 * s, 1.2 * s)
            ctx.lineTo(10.6 * s, 5.2 * s)
            ctx.lineTo(14.6 * s, 5.2 * s)
            ctx.stroke()
        }
    }
}
