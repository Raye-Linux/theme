// A pinned app: 32 px icon, the name under it on two lines at most, a
// backplate with 4 px corners when the pointer is on it or the keys are.
// The grid around it does the clicks, the drag and the menu.
import QtQuick
import org.kde.kirigami as Kirigami
import org.raye.ui
import org.raye.shell

Item {
    id: tile

    property string name: ""
    property var icon: ""
    property bool hovered: false
    property bool current: false
    property bool pressed: false
    property bool dragged: false

    Rectangle {
        anchors { fill: parent; margins: 2 }
        radius: Tokens.radiusControl
        color: tile.pressed ? ShellTokens.pressFill
                            : tile.hovered || tile.dragged ? ShellTokens.hoverFill : "transparent"
        border.width: tile.dragged ? 1 : 0
        border.color: Tokens.strokeControl
        Behavior on color { ColorAnimation { duration: ShellTokens.durationFaster } }

        FocusRing {
            radius: Tokens.radiusControl
            shown: tile.current && tile.activeFocusShown
        }
    }
    property bool activeFocusShown: false

    Kirigami.Icon {
        id: glyph
        anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 14 }
        width: 32
        height: 32
        source: tile.icon
        scale: tile.pressed ? 0.9 : 1
        Behavior on scale { NumberAnimation { duration: ShellTokens.durationFaster; easing: Motion.pointToPoint } }
    }

    Text {
        anchors { top: glyph.bottom; topMargin: 8; left: parent.left; right: parent.right; leftMargin: 6; rightMargin: 6 }
        text: tile.name
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        maximumLineCount: 2
        elide: Text.ElideRight
        font: Tokens.caption
        color: ShellTokens.text
        textFormat: Text.PlainText
    }
}
