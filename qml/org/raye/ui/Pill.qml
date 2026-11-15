// The selection pill: 3 x 16 px, accent, at the left of the selected row
// of a list or a navigation pane. It grows from the middle when the row
// gets selected and shrinks away when it loses it.
//
//   Pill { active: row.selected; anchors.verticalCenter: parent.verticalCenter; x: 4 }
import QtQuick
import org.raye.ui

Rectangle {
    id: pill

    property bool active: true
    // the row hovered or pressed: Windows shortens the pill a little
    property bool pressed: false

    readonly property real fullHeight: pressed ? Tokens.sizePillHeight - 6 : Tokens.sizePillHeight

    implicitWidth: Tokens.sizePillWidth
    implicitHeight: Tokens.sizePillHeight
    width: Tokens.sizePillWidth
    height: active ? fullHeight : 0
    radius: Tokens.radiusNavPill
    color: Tokens.accent
    opacity: active ? 1 : 0
    Accessible.ignored: true

    Behavior on height {
        NumberAnimation { duration: Tokens.durationFast; easing.type: Tokens.easingFast }
    }
    Behavior on opacity {
        NumberAnimation { duration: Tokens.durationFaster; easing.type: Tokens.easingFaster }
    }
}
