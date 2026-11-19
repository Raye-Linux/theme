// A tile of the Windows 10 Start: the app's icon on the accent, its name
// at the bottom left (not on a small tile). A light frame on hover, a
// little smaller while pressed. Dragged, it follows the pointer and the
// tile area decides where it lands.
import QtQuick
import org.kde.kirigami as Kirigami
import org.raye.ui
import org.raye.shell

Item {
    id: tile

    property string favoriteId: ""
    property string name: ""
    property var iconSource: ""
    property string size: "medium"
    property bool keyboardCurrent: false
    // while dragged: how far from its place
    property point dragOffset: Qt.point(0, 0)
    readonly property bool dragging: mouse.dragging
    readonly property bool hovered: mouse.containsMouse

    signal launch()
    signal menuRequested(point at)
    // the pointer, in the tile's own coordinates, while it is dragged
    signal dragMoved(point at)
    signal dropped(point at)

    objectName: "tile"
    Accessible.role: Accessible.Button
    Accessible.name: name
    Accessible.onPressAction: tile.launch()
    z: dragging ? 10 : 0

    readonly property int iconSize: size === "small" ? 24 : size === "large" ? 72 : 40

    Item {
        id: face
        width: tile.width
        height: tile.height
        x: tile.dragOffset.x
        y: tile.dragOffset.y
        scale: mouse.pressed && !mouse.dragging ? 0.97 : 1
        opacity: tile.dragging ? 0.85 : 1
        Behavior on scale { NumberAnimation { duration: ShellTokens.durationFaster; easing: Motion.pointToPoint } }

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0; color: Qt.lighter(ShellTokens.accent, 1.12) }
                GradientStop { position: 1; color: ShellTokens.accent }
            }
        }
        // the light frame of a tile under the pointer
        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.width: 2
            border.color: Qt.rgba(1, 1, 1, tile.hovered ? 0.45 : 0)
            Behavior on border.color { ColorAnimation { duration: ShellTokens.durationFaster } }
        }
        FocusRing {
            radius: 0
            shown: tile.keyboardCurrent
        }

        Kirigami.Icon {
            objectName: "tileIcon"
            anchors.centerIn: parent
            anchors.verticalCenterOffset: tile.size === "small" ? 0 : -6
            width: tile.iconSize
            height: tile.iconSize
            source: tile.iconSource
        }
        Text {
            objectName: "tileName"
            visible: tile.size !== "small"
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom
                      leftMargin: 8; rightMargin: 8; bottomMargin: 6 }
            text: tile.name
            elide: Text.ElideRight
            color: ShellTokens.textOnAccent
            font: Tokens.caption
            textFormat: Text.PlainText
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        // the tiles area scrolls; a drag that starts on a tile is the tile's
        preventStealing: true
        property point pressAt: Qt.point(0, 0)
        property bool dragging: false
        property bool moved: false

        onPressed: event => {
            pressAt = Qt.point(event.x, event.y)
            dragging = false
            moved = false
        }
        onPositionChanged: event => {
            if (!pressed || !(pressedButtons & Qt.LeftButton))
                return
            const dx = event.x - pressAt.x, dy = event.y - pressAt.y
            if (!dragging && Math.hypot(dx, dy) > Qt.styleHints.startDragDistance)
                dragging = true
            if (dragging) {
                tile.dragOffset = Qt.point(dx, dy)
                tile.dragMoved(Qt.point(event.x, event.y))
            }
        }
        onReleased: event => {
            if (dragging) {
                moved = true
                dragging = false
                tile.dragOffset = Qt.point(0, 0)
                tile.dropped(Qt.point(event.x, event.y))
            }
        }
        onCanceled: {
            dragging = false
            tile.dragOffset = Qt.point(0, 0)
        }
        onClicked: event => {
            if (moved)
                return
            if (event.button === Qt.RightButton)
                tile.menuRequested(mapToGlobal(event.x, event.y))
            else
                tile.launch()
        }
    }
}
