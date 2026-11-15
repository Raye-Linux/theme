// A tile of the grid. In Edit it stops doing its job: it can be dragged to
// another place, and its badge takes it out of the panel.
import QtQuick
import org.kde.kirigami as Kirigami
import org.raye.shell

Item {
    id: cell

    property string tileId: ""
    property string title: ""
    // what shows under the tile in place of its name (the network)
    property string subtitle: ""
    property string iconName: ""
    property bool checked: false
    property bool split: false
    property bool editing: false

    signal toggled()
    signal expandRequested()
    signal menuRequested(real x, real y)
    signal removeRequested()
    signal dropped(real x, real y)

    implicitWidth: tile.implicitWidth
    implicitHeight: tile.implicitHeight

    RayeToggleTile {
        id: tile
        objectName: "tile-" + cell.tileId
        text: cell.title
        subtitle: cell.subtitle
        iconName: cell.iconName
        checked: cell.checked
        split: cell.split
        opacity: cell.editing ? 0.85 : 1
        z: dragArea.drag.active ? 10 : 0
        onToggled: cell.toggled()
        onExpandRequested: cell.expandRequested()
        onMenuRequested: (x, y) => { if (!cell.editing) cell.menuRequested(tile.x + x, tile.y + y) }
    }

    AccessibilityGlyph {
        visible: cell.tileId === "accessibility"
        x: tile.x + (ShellTokens.toggleTileWidth - width) / 2
        y: tile.y + (ShellTokens.toggleTileHeight - height) / 2
        color: tile.ink
    }

    MouseArea {
        id: dragArea
        visible: cell.editing
        anchors.fill: tile
        drag.target: tile
        drag.smoothed: false
        cursorShape: drag.active ? Qt.ClosedHandCursor : Qt.OpenHandCursor
        onReleased: {
            if (drag.active)
                cell.dropped(cell.x + tile.x + tile.width / 2, cell.y + tile.y + tile.height / 2)
            tile.x = 0
            tile.y = 0
        }
    }

    // the badge that takes the tile out
    Rectangle {
        objectName: "unpin-" + cell.tileId
        visible: cell.editing
        x: tile.x + ShellTokens.toggleTileWidth - width / 2 - 2
        y: tile.y - height / 2 + 2
        width: 18
        height: 18
        radius: 9
        z: 11
        color: ShellTokens.acrylicSolid
        border.width: 1
        border.color: ShellTokens.separator
        Rectangle {
            anchors.centerIn: parent
            width: 8
            height: 1.5
            color: ShellTokens.text
        }
        MouseArea {
            anchors.fill: parent
            anchors.margins: -4
            onClicked: cell.removeRequested()
            Accessible.role: Accessible.Button
            Accessible.name: i18nd("raye_shell_quicksettings", "Remove %1", cell.title)
        }
    }
}
