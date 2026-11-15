// The Raye acrylic of a shell surface (Start, quick settings, menus,
// flyouts): the Night or Day tint over a blur of what lies behind the
// window, a 1 px light line inside the edge and, on the surface that has
// the focus, a faint violet rim. Without blur (turned off, or a
// compositor that cannot) the tint is drawn opaque, so text stays legible.
//
//   AcrylicSurface { anchors.fill: parent; focused: popup.activeFocus }
//
// Its shadow comes from the window around it (Plasma's dialog frame).
import QtQuick
import org.raye.shell

Item {
    id: surface

    property real radius: ShellTokens.radiusFlyout
    property bool focused: false
    // ask the compositor for the blur behind the window
    property bool blur: true
    // the blur is really there
    readonly property bool frosted: blurBehind.active

    BlurBehind {
        id: blurBehind
        item: surface
        radius: surface.radius
        enabled: surface.blur && surface.visible
    }

    Rectangle {
        objectName: "acrylicTint"
        anchors.fill: parent
        radius: surface.radius
        color: surface.frosted ? ShellTokens.acrylic : ShellTokens.acrylicSolid
        Behavior on color { ColorAnimation { duration: ShellTokens.durationFast } }
    }

    // a touch of light at the top edge, the way glass catches it
    Rectangle {
        anchors.fill: parent
        radius: surface.radius
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 0.04) }
            GradientStop { position: 0.35; color: "transparent" }
        }
    }

    Rectangle {
        objectName: "acrylicStroke"
        anchors.fill: parent
        radius: surface.radius
        color: "transparent"
        border.width: 1
        border.color: surface.focused ? ShellTokens.focusRim : ShellTokens.stroke
        Behavior on border.color { ColorAnimation { duration: ShellTokens.durationFast } }
    }
}
