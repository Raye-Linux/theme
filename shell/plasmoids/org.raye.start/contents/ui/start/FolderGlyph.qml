// The glyph of a folder button next to power, drawn in outline on a 24 px
// grid: a folder for Files, a page for Documents, an arrow into a tray for
// Downloads, and so on (Menu.folderGlyph names them). Each folder has its
// own picture, so two buttons never look the same.
import QtQuick

Canvas {
    id: glyph

    property string kind: ""
    property color color: "white"

    implicitWidth: 20
    implicitHeight: 20
    onKindChanged: requestPaint()
    onColorChanged: requestPaint()

    function roundRect(ctx, x, y, w, h, r) {
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
        ctx.scale(s, s)
        ctx.lineWidth = 1.6
        ctx.lineCap = "round"
        ctx.lineJoin = "round"
        ctx.strokeStyle = glyph.color
        ctx.fillStyle = glyph.color
        ctx.beginPath()
        switch (kind) {
        case "folder":
            ctx.moveTo(3, 18)
            ctx.lineTo(3, 6.5)
            ctx.arcTo(3, 5, 4.5, 5, 1.5)
            ctx.lineTo(9, 5)
            ctx.lineTo(11, 7.5)
            ctx.lineTo(19.5, 7.5)
            ctx.arcTo(21, 7.5, 21, 9, 1.5)
            ctx.lineTo(21, 18)
            ctx.arcTo(21, 19.5, 19.5, 19.5, 1.5)
            ctx.lineTo(4.5, 19.5)
            ctx.arcTo(3, 19.5, 3, 18, 1.5)
            ctx.moveTo(3, 10.5)
            ctx.lineTo(21, 10.5)
            ctx.stroke()
            break
        case "document":
            ctx.moveTo(14, 3)
            ctx.lineTo(7, 3)
            ctx.arcTo(5, 3, 5, 5, 2)
            ctx.lineTo(5, 19)
            ctx.arcTo(5, 21, 7, 21, 2)
            ctx.lineTo(17, 21)
            ctx.arcTo(19, 21, 19, 19, 2)
            ctx.lineTo(19, 8)
            ctx.closePath()
            ctx.moveTo(14, 3)
            ctx.lineTo(14, 8)
            ctx.lineTo(19, 8)
            ctx.moveTo(8.5, 12.5)
            ctx.lineTo(15.5, 12.5)
            ctx.moveTo(8.5, 16.5)
            ctx.lineTo(13.5, 16.5)
            ctx.stroke()
            break
        case "download":
            ctx.moveTo(12, 3.5)
            ctx.lineTo(12, 15)
            ctx.moveTo(7, 10.5)
            ctx.lineTo(12, 15.5)
            ctx.lineTo(17, 10.5)
            ctx.moveTo(4.5, 20)
            ctx.lineTo(19.5, 20)
            ctx.stroke()
            break
        case "music":
            ctx.moveTo(9.5, 17.5)
            ctx.lineTo(9.5, 5.5)
            ctx.lineTo(19, 3.5)
            ctx.lineTo(19, 15.5)
            ctx.moveTo(9.5, 9.5)
            ctx.lineTo(19, 7.5)
            ctx.stroke()
            ctx.beginPath()
            ctx.ellipse(4, 15, 5.5, 5)
            ctx.ellipse(13.5, 13, 5.5, 5)
            ctx.stroke()
            break
        case "picture":
            roundRect(ctx, 3, 4.5, 18, 15, 2)
            ctx.moveTo(3.5, 17)
            ctx.lineTo(9, 11.5)
            ctx.lineTo(14, 16.5)
            ctx.moveTo(13, 15.5)
            ctx.lineTo(15.5, 13)
            ctx.lineTo(20.5, 18)
            ctx.stroke()
            ctx.beginPath()
            ctx.arc(15.5, 9, 1.6, 0, 2 * Math.PI)
            ctx.fill()
            break
        case "video":
            roundRect(ctx, 2.5, 6, 13, 12, 2)
            ctx.moveTo(15.5, 10.5)
            ctx.lineTo(21, 7.5)
            ctx.lineTo(21, 16.5)
            ctx.lineTo(15.5, 13.5)
            ctx.stroke()
            break
        case "network":
            ctx.arc(12, 12, 9, 0, 2 * Math.PI)
            ctx.moveTo(3, 12)
            ctx.lineTo(21, 12)
            ctx.stroke()
            ctx.beginPath()
            ctx.ellipse(8, 3, 8, 18)
            ctx.stroke()
            break
        case "person":
            ctx.arc(12, 8, 4, 0, 2 * Math.PI)
            ctx.moveTo(4.5, 20.5)
            ctx.bezierCurveTo(5, 16, 8, 14, 12, 14)
            ctx.bezierCurveTo(16, 14, 19, 16, 19.5, 20.5)
            ctx.stroke()
            break
        }
    }
}
