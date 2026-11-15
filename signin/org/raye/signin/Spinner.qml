// The Raye progress ring, the same as on the boot screen: one arc on a
// faint track that grows, shrinks and turns all the time, violet at the
// tail to magenta at the head, one loop in 2 s. Drawn as a path, so it
// is sharp at any size and scale. With reduced motion it stands still.
import QtQuick
import QtQuick.Shapes

Item {
    id: spinner

    property int size: 32
    property real lineWidth: Math.max(2, Math.round(size * 0.21) / 2)
    property bool running: visible

    implicitWidth: size
    implicitHeight: size

    Accessible.role: Accessible.Animation
    Accessible.name: Strings.tr.pleaseWait

    // Where the loop is, 0 to 1. Standing still it shows a long arc.
    property real phase: 0.45

    NumberAnimation on phase {
        from: 0
        to: 1
        duration: 2000
        loops: Animation.Infinite
        running: spinner.running && !Motion.reducedMotion
    }

    // art/plymouth.py draws the boot screen frames with the same numbers:
    // the head gains 270 degrees over the first 60 % of the loop, the
    // tail over the last 60 %, with cubic-bezier(0.6, 0, 0.3, 1), and the
    // whole ring turns 1.25 times on top of that.
    function ease(x) {
        if (x <= 0) return 0
        if (x >= 1) return 1
        var lo = 0, hi = 1, t = 0.5
        for (var i = 0; i < 20; i++) {
            t = (lo + hi) / 2
            var bx = 1.8 * t * (1 - t) * (1 - t) + 0.9 * t * t * (1 - t) + t * t * t
            if (bx < x) lo = t
            else hi = t
        }
        return 3 * t * t * (1 - t) + t * t * t
    }

    readonly property real _turn: 450 * phase
    readonly property real _head: _turn + 12 + 270 * ease(phase / 0.6)
    readonly property real _tail: _turn + 270 * ease((phase - 0.4) / 0.6)
    readonly property real _len: _head - _tail

    readonly property real _c: size / 2
    readonly property real _outer: size / 2
    readonly property real _inner: size / 2 - lineWidth
    // how far a round end reaches past its point, in degrees
    readonly property real _cap: lineWidth / 2 / (size / 2 - lineWidth / 2) * 180 / Math.PI

    readonly property color _headColour: RayePalette.highContrast ? "#ffffff" : RayePalette.glow
    readonly property color _tailColour: RayePalette.highContrast ? "#ffffff" : RayePalette.accent

    function _x(r, deg) { return _c + r * Math.sin(deg * Math.PI / 180) }
    function _y(r, deg) { return _c - r * Math.cos(deg * Math.PI / 180) }

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: "transparent"
            strokeColor: Qt.rgba(spinner._tailColour.r, spinner._tailColour.g, spinner._tailColour.b,
                                 RayePalette.highContrast ? 0.3 : 0.16)
            strokeWidth: spinner.lineWidth
            PathAngleArc {
                centerX: spinner._c
                centerY: spinner._c
                radiusX: spinner._outer - spinner.lineWidth / 2
                radiusY: radiusX
                startAngle: 0
                sweepAngle: 360
            }
        }

        // the arc is filled, not stroked, so its colour can run along it:
        // outer edge from tail to head, round end, inner edge back, round end
        ShapePath {
            strokeColor: "transparent"
            strokeWidth: -1
            fillGradient: ConicalGradient {
                centerX: spinner._c
                centerY: spinner._c
                // counter-clockwise from 3 o'clock, starting just past the head
                angle: 90 - spinner._head - spinner._cap
                GradientStop { position: 0; color: spinner._headColour }
                GradientStop { position: spinner._cap / 360; color: spinner._headColour }
                GradientStop { position: (spinner._cap + spinner._len) / 360; color: spinner._tailColour }
                GradientStop { position: 1; color: spinner._tailColour }
            }

            PathAngleArc {
                centerX: spinner._c
                centerY: spinner._c
                radiusX: spinner._outer
                radiusY: spinner._outer
                startAngle: spinner._tail - 90
                sweepAngle: spinner._len
            }
            PathArc {
                x: spinner._x(spinner._inner, spinner._head)
                y: spinner._y(spinner._inner, spinner._head)
                radiusX: spinner.lineWidth / 2
                radiusY: spinner.lineWidth / 2
            }
            PathAngleArc {
                centerX: spinner._c
                centerY: spinner._c
                radiusX: spinner._inner
                radiusY: spinner._inner
                startAngle: spinner._head - 90
                sweepAngle: -spinner._len
                moveToStart: false
            }
            PathArc {
                x: spinner._x(spinner._outer, spinner._tail)
                y: spinner._y(spinner._outer, spinner._tail)
                radiusX: spinner.lineWidth / 2
                radiusY: spinner.lineWidth / 2
            }
        }
    }
}
