// The Task view glyph, drawn: two rounded squares, the front one over
// the back one. The front square slides 2 px when the pointer is over it.
import QtQuick

Canvas {
    id: glyph

    property color color: "white"
    property color backColor: Qt.alpha(color, 0.6)
    property real slide: 0

    implicitWidth: 24
    implicitHeight: 24
    onColorChanged: requestPaint()
    onSlideChanged: requestPaint()

    function roundRect(ctx, x, y, w, h, r) {
        ctx.beginPath()
        ctx.moveTo(x + r, y)
        ctx.arcTo(x + w, y, x + w, y + h, r)
        ctx.arcTo(x + w, y + h, x, y + h, r)
        ctx.arcTo(x, y + h, x, y, r)
        ctx.arcTo(x, y, x + w, y, r)
        ctx.closePath()
    }

    onPaint: {
        const ctx = getContext("2d")
        const s = Math.min(width, height) / 24
        ctx.reset()
        ctx.lineWidth = 1.5 * s
        // the back square, only where the front one does not cover it
        ctx.save()
        ctx.beginPath()
        ctx.rect(0, 0, width, height)
        ctx.rect((8 + slide) * s, (9 - slide) * s, 14 * s, 12 * s)
        ctx.fillRule = Qt.OddEvenFill
        ctx.clip()
        ctx.strokeStyle = glyph.backColor
        roundRect(ctx, 3 * s, 4 * s, 14 * s, 12 * s, 2.5 * s)
        ctx.stroke()
        ctx.restore()
        ctx.strokeStyle = glyph.color
        roundRect(ctx, (8.75 + slide) * s, (9.75 - slide) * s, 12.5 * s, 10.5 * s, 2.5 * s)
        ctx.stroke()
    }
}
