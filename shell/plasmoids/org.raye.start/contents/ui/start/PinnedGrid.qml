// The Pinned grid: 6 tiles per row, 3 or 4 rows a page, page dots on the
// right (the wheel and Page Up/Down turn pages). Drag a tile to move it:
// the others make room as it passes, the order is saved when it is let
// go. Right-click (or the Menu key) opens the app's menu.
import QtQuick
import org.raye.ui
import org.raye.shell

import "../menulogic.js" as Menu

FocusScope {
    id: grid

    // Kicker SimpleFavoritesModel (display, decoration, favoriteId)
    property var model: null
    property int rows: 3
    readonly property int perPage: rows * Menu.columns
    readonly property int count: tiles.count
    readonly property int pages: Menu.pageCount(count, perPage)
    property int page: 0
    // the tile the keys are on
    property int currentIndex: 0
    property bool keysUsed: false
    readonly property real cellWidth: Math.floor(width / Menu.columns)
    readonly property real cellHeight: Menu.cellHeight

    // while a tile is dragged: which, and over which place
    property int dragFrom: -1
    property int dragTo: -1
    property point dragPoint: Qt.point(0, 0)

    signal launch(int row)
    signal moved(int from, int to)
    signal menuRequested(int row, string favoriteId, point at)

    implicitHeight: rows * cellHeight
    clip: false

    onCountChanged: if (page >= pages) page = pages - 1
    onRowsChanged: if (page >= pages) page = pages - 1

    function tileAt(i) {
        return tiles.itemAt(i)
    }
    function showCurrent() {
        page = Math.floor(currentIndex / perPage)
    }
    function moveCurrent(step) {
        if (count === 0)
            return
        keysUsed = true
        currentIndex = Math.max(0, Math.min(count - 1, currentIndex + step))
        showCurrent()
    }

    Keys.onPressed: event => {
        switch (event.key) {
        case Qt.Key_Left: moveCurrent(-1); break
        case Qt.Key_Right: moveCurrent(1); break
        case Qt.Key_Up:
            if (currentIndex - Menu.columns < 0) { event.accepted = false; return }
            moveCurrent(-Menu.columns); break
        case Qt.Key_Down:
            if (currentIndex + Menu.columns >= count) { event.accepted = false; return }
            moveCurrent(Menu.columns); break
        case Qt.Key_PageUp: page = Math.max(0, page - 1); currentIndex = page * perPage; break
        case Qt.Key_PageDown: page = Math.min(pages - 1, page + 1); currentIndex = page * perPage; break
        case Qt.Key_Return:
        case Qt.Key_Enter:
            if (count > 0) grid.launch(currentIndex)
            break
        case Qt.Key_Menu: {
            const t = tileAt(currentIndex)
            if (t) grid.menuRequested(currentIndex, t.favoriteId, t.mapToGlobal(t.width / 2, t.height / 2))
            break
        }
        default:
            event.accepted = false
            return
        }
        event.accepted = true
    }

    Item {
        id: area
        anchors { left: parent.left; right: parent.right; top: parent.top; rightMargin: grid.pages > 1 ? 0 : 0 }
        height: grid.rows * grid.cellHeight

        Repeater {
            id: tiles
            model: grid.model
            delegate: AppTile {
                id: tile
                required property int index
                required property var model
                readonly property string favoriteId: model.favoriteId || ""
                readonly property int place: Menu.visualIndex(index, grid.dragFrom, grid.dragTo)
                readonly property bool isDragged: index === grid.dragFrom
                readonly property int onPage: Math.floor(place / grid.perPage)

                objectName: "pinnedTile"
                name: model.display || ""
                icon: model.decoration || ""
                width: grid.cellWidth
                height: grid.cellHeight
                visible: onPage === grid.page || isDragged
                z: isDragged ? 10 : 0
                x: isDragged ? grid.dragPoint.x - width / 2 : (place % Menu.columns) * grid.cellWidth
                y: isDragged ? grid.dragPoint.y - height / 2 : Math.floor((place % grid.perPage) / Menu.columns) * grid.cellHeight
                hovered: mouse.hoveredIndex === index && grid.dragFrom < 0
                pressed: mouse.pressedIndex === index && mouse.pressed && grid.dragFrom < 0
                dragged: isDragged
                current: grid.currentIndex === index
                activeFocusShown: grid.activeFocus && grid.keysUsed
                opacity: isDragged ? 0.9 : 1
                Behavior on x { enabled: !tile.isDragged && grid.dragFrom >= 0; NumberAnimation { duration: ShellTokens.durationFast; easing: Motion.pointToPoint } }
                Behavior on y { enabled: !tile.isDragged && grid.dragFrom >= 0; NumberAnimation { duration: ShellTokens.durationFast; easing: Motion.pointToPoint } }
                Accessible.role: Accessible.Button
                Accessible.name: name
            }
        }

        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            property int hoveredIndex: -1
            property int pressedIndex: -1
            property point pressPoint

            function indexAt(x, y) {
                const slot = Menu.slotAt(x, y, grid.page, grid.perPage, grid.count + 1, grid.cellWidth, grid.cellHeight)
                // the slot shows the tile whose place it is
                for (let i = 0; i < tiles.count; ++i) {
                    const t = tiles.itemAt(i)
                    if (t && t.place === slot && t.visible)
                        return i
                }
                return -1
            }

            onPositionChanged: mouse => {
                if (pressed && pressedIndex >= 0 && (mouse.buttons & Qt.LeftButton)) {
                    const far = Math.abs(mouse.x - pressPoint.x) + Math.abs(mouse.y - pressPoint.y) > 8
                    if (grid.dragFrom < 0 && far) {
                        grid.dragFrom = pressedIndex
                        grid.dragTo = pressedIndex
                    }
                    if (grid.dragFrom >= 0) {
                        grid.dragPoint = Qt.point(mouse.x, mouse.y)
                        grid.dragTo = Menu.slotAt(mouse.x, mouse.y, grid.page, grid.perPage, grid.count,
                                                  grid.cellWidth, grid.cellHeight)
                    }
                    return
                }
                hoveredIndex = indexAt(mouse.x, mouse.y)
            }
            onExited: hoveredIndex = -1
            onPressed: mouse => {
                pressPoint = Qt.point(mouse.x, mouse.y)
                pressedIndex = indexAt(mouse.x, mouse.y)
                grid.keysUsed = false
                if (pressedIndex >= 0)
                    grid.currentIndex = pressedIndex
            }
            onReleased: mouse => {
                if (grid.dragFrom >= 0) {
                    const from = grid.dragFrom
                    const to = grid.dragTo
                    grid.dragFrom = -1
                    grid.dragTo = -1
                    if (from !== to)
                        grid.moved(from, to)
                } else if (pressedIndex >= 0 && pressedIndex === indexAt(mouse.x, mouse.y)) {
                    if (mouse.button === Qt.RightButton) {
                        const t = tiles.itemAt(pressedIndex)
                        grid.menuRequested(pressedIndex, t ? t.favoriteId : "", mapToGlobal(mouse.x, mouse.y))
                    } else {
                        grid.launch(pressedIndex)
                    }
                }
                pressedIndex = -1
            }
            onCanceled: {
                grid.dragFrom = -1
                grid.dragTo = -1
                pressedIndex = -1
            }
            onWheel: wheel => {
                if (wheel.angleDelta.y < 0 && grid.page < grid.pages - 1)
                    grid.page++
                else if (wheel.angleDelta.y > 0 && grid.page > 0)
                    grid.page--
            }
        }

        // a dragged tile held at the bottom or top edge turns the page
        Timer {
            interval: 600
            repeat: true
            running: grid.dragFrom >= 0
            onTriggered: {
                if (grid.dragPoint.y > area.height - 12 && grid.page < grid.pages - 1)
                    grid.page++
                else if (grid.dragPoint.y < 12 && grid.page > 0)
                    grid.page--
            }
        }
    }

    // page dots, at the right edge, when there is more than one page
    Column {
        objectName: "pageDots"
        anchors { right: parent.right; rightMargin: -20; verticalCenter: area.verticalCenter }
        spacing: 6
        visible: grid.pages > 1
        Repeater {
            model: grid.pages
            delegate: Rectangle {
                required property int index
                width: 6
                height: index === grid.page ? 14 : 6
                radius: 3
                color: index === grid.page ? ShellTokens.accent : Qt.alpha(ShellTokens.text, dotMouse.containsMouse ? 0.6 : 0.35)
                Behavior on height { NumberAnimation { duration: ShellTokens.durationFast; easing: Motion.pointToPoint } }
                MouseArea {
                    id: dotMouse
                    anchors { fill: parent; margins: -4 }
                    hoverEnabled: true
                    onClicked: grid.page = parent.index
                }
            }
        }
    }
}
