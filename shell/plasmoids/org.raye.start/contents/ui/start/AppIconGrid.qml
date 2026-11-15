// Apps as icons, six to a row, the way Pinned draws them: the Grid view of
// All, and an open category. Arrows move, Return starts, the Menu key or a
// right-click gives the app's menu.
import QtQuick
import QtQuick.Controls as QQC
import org.raye.shell

import "../menulogic.js" as Menu

GridView {
    id: grid

    property var apps: []
    property string tileName: "allGridTile"
    property bool keysUsed: false

    signal launch(var app)
    signal menuRequested(var app, point at)

    clip: true
    model: apps
    cellWidth: Math.floor((width - 12) / Menu.columns)
    cellHeight: Menu.cellHeight
    boundsBehavior: Flickable.StopAtBounds
    keyNavigationEnabled: false
    currentIndex: 0
    QQC.ScrollBar.vertical: QQC.ScrollBar { policy: QQC.ScrollBar.AsNeeded }

    function step(d) {
        const next = currentIndex + d
        if (next < 0 || next >= apps.length)
            return false
        keysUsed = true
        currentIndex = next
        positionViewAtIndex(next, GridView.Contain)
        return true
    }

    Keys.onPressed: event => {
        if (event.key === Qt.Key_Right) event.accepted = step(1)
        else if (event.key === Qt.Key_Left) event.accepted = step(-1)
        else if (event.key === Qt.Key_Down) event.accepted = step(Menu.columns)
        else if (event.key === Qt.Key_Up) event.accepted = step(-Menu.columns)
        else if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && apps[currentIndex]) {
            launch(apps[currentIndex])
            event.accepted = true
        } else if (event.key === Qt.Key_Menu && currentItem && apps[currentIndex]) {
            menuRequested(apps[currentIndex], currentItem.mapToGlobal(40, 40))
            event.accepted = true
        } else event.accepted = false
    }

    delegate: AppTile {
        id: tile
        required property int index
        required property var modelData
        objectName: grid.tileName
        width: grid.cellWidth
        height: grid.cellHeight
        name: modelData.name
        icon: modelData.icon
        hovered: tileMouse.containsMouse
        pressed: tileMouse.pressed
        current: GridView.isCurrentItem
        activeFocusShown: grid.activeFocus && grid.keysUsed
        MouseArea {
            id: tileMouse
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: mouse => {
                grid.keysUsed = false
                grid.currentIndex = tile.index
                if (mouse.button === Qt.RightButton)
                    grid.menuRequested(tile.modelData, mapToGlobal(mouse.x, mouse.y))
                else
                    grid.launch(tile.modelData)
            }
        }
    }
}
