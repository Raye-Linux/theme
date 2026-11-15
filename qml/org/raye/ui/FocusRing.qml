// The keyboard focus ring: 2 px outer ring and 1 px inner ring drawn
// outside the control, the corner radius 3 px larger than the control's,
// and a faint violet glow around it at night. Shown only when the focus
// came from the keyboard, never after a click.
//
//   AccentButton { FocusRing { radius: Tokens.radiusControl } }
//
// It fills its parent (the control) and grows out of it; the parent must
// not clip.
import QtQuick
import org.raye.ui

Item {
    id: ring

    // the control's own corner radius
    property real radius: Tokens.radiusControl
    // the control: its visualFocus (keyboard only) decides, when it has one
    property Item target: parent
    property bool shown: target ? (target.visualFocus !== undefined ? target.visualFocus
                                                                    : target.activeFocus)
                                : false

    readonly property int gap: Tokens.sizeFocusOuter + Tokens.sizeFocusInner

    anchors.fill: parent
    anchors.margins: -gap
    visible: shown
    z: 100
    Accessible.ignored: true

    Rectangle {
        // the glow, outside the outer ring
        anchors.fill: parent
        anchors.margins: -Tokens.sizeFocusGlow
        visible: Tokens.night
        radius: ring.radius + ring.gap + Tokens.sizeFocusGlow
        color: "transparent"
        border.width: Tokens.sizeFocusGlow
        border.color: Tokens.glow
    }
    Rectangle {
        anchors.fill: parent
        radius: ring.radius + ring.gap
        color: "transparent"
        border.width: Tokens.sizeFocusOuter
        border.color: Tokens.focusOuter
    }
    Rectangle {
        anchors.fill: parent
        anchors.margins: Tokens.sizeFocusOuter
        radius: ring.radius + Tokens.sizeFocusInner
        color: "transparent"
        border.width: Tokens.sizeFocusInner
        border.color: Tokens.focusInner
    }
}
