// A round account picture. Without a picture (or when it fails to load)
// the Raye default face shows.
//
// The big sign-in picture (192 px) has a thin light ring and a soft
// violet glow behind it; the small ones of the user list (40 px) have
// neither. Drawn with a Canvas so the circle is smooth with or without a
// GPU (MultiEffect masks do not draw under the software scene graph).
import QtQuick

Item {
    id: root

    property url source: ""
    property int size: 192
    property bool ring: size >= 96
    property bool glow: size >= 96

    readonly property url shown: (source.toString() === "" || probe.status === Image.Error)
                                 ? RayePalette.defaultFace : source
    readonly property real dpr: Screen.devicePixelRatio

    implicitWidth: size
    implicitHeight: size

    Accessible.role: Accessible.Graphic
    Accessible.ignored: true

    // only to learn whether the picture loads
    Image {
        id: probe
        visible: false
        source: root.source
        sourceSize: Qt.size(root.size * root.dpr, root.size * root.dpr)
        asynchronous: true
    }

    // the violet glow, a radial gradient behind the circle
    Canvas {
        id: halo
        visible: root.glow && !RayePalette.highContrast
        anchors.centerIn: parent
        width: root.size + 64
        height: root.size + 64
        onWidthChanged: requestPaint()
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            var c = width / 2
            var g = ctx.createRadialGradient(c, c, root.size / 2 - 8, c, c, c)
            g.addColorStop(0.0, Qt.rgba(0xc0 / 255, 0x40 / 255, 1, 0.30))
            g.addColorStop(0.45, Qt.rgba(0x7b / 255, 0x3c / 255, 1, 0.12))
            g.addColorStop(1.0, Qt.rgba(0x7b / 255, 0x3c / 255, 1, 0))
            ctx.fillStyle = g
            ctx.beginPath()
            ctx.arc(c, c, c, 0, Math.PI * 2)
            ctx.fill()
        }
    }

    Canvas {
        id: disc
        anchors.fill: parent
        antialiasing: true

        property string loaded: ""

        function load() {
            var url = root.shown.toString()
            if (url === "")
                return
            if (isImageLoaded(url))
                requestPaint()
            else
                loadImage(url)
        }

        Component.onCompleted: load()
        onImageLoaded: requestPaint()
        onWidthChanged: requestPaint()
        Connections {
            target: root
            function onShownChanged() { disc.load() }
        }

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            var r = width / 2
            var url = root.shown.toString()
            var image = isImageLoaded(url) ? ctx.createImageData(url) : null
            ctx.beginPath()
            ctx.arc(r, r, r, 0, Math.PI * 2)
            if (!image || image.width <= 0 || image.height <= 0) {
                ctx.fillStyle = RayePalette.nightHigh
                ctx.fill()
                return
            }
            // cover the circle, centred, like PreserveAspectCrop
            var iw = image.width
            var ih = image.height
            var s = Math.max(width / iw, height / ih)
            var dx = (width - iw * s) / 2
            var dy = (height - ih * s) / 2
            ctx.save()
            ctx.translate(dx, dy)
            ctx.scale(s, s)
            ctx.fillStyle = ctx.createPattern(url, "no-repeat")
            ctx.beginPath()
            ctx.arc((r - dx) / s, (r - dy) / s, r / s, 0, Math.PI * 2)
            ctx.fill()
            ctx.restore()
        }
    }

    Rectangle {
        visible: root.ring
        anchors.fill: parent
        radius: width / 2
        color: "transparent"
        border.width: 2
        border.color: RayePalette.ring
    }
}
