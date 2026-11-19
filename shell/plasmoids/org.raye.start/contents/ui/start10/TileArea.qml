// The tiles of the Windows 10 Start: groups of tiles under their names,
// in one to three columns. Click a name to change it. Drag a tile onto
// another to put it there, onto a group to put it at its end, or below
// the groups to start a new group. Right-click a tile to resize it.
// Every change goes out through `edited`; the view writes [Start] Tiles.
import QtQuick
import org.kde.kirigami as Kirigami
import org.raye.ui
import org.raye.shell

import "../tileslogic.js" as Tiles

FocusScope {
    id: area

    // tileslogic.js groups, and what is known of each pinned app:
    //   info[id] = { name, icon, row }
    property var groups: []
    property var info: ({})
    property int columns: 1
    property var backend: null

    signal edited(var groups)
    signal launch(string id)
    signal menuRequested(string id, string size, point at)

    readonly property int pitch: Tiles.cell + Tiles.gap
    readonly property int headerHeight: 40
    readonly property int groupSpace: 24
    readonly property var packed: groups.map(g => Tiles.pack(g.tiles))
    readonly property var heights: packed.map(p => headerHeight + Math.max(0, p.rows * pitch - Tiles.gap) + groupSpace)
    readonly property var places: {
        const col = Tiles.columnOf(heights, columns)
        const fill = []
        for (let c = 0; c < columns; ++c)
            fill.push(0)
        const out = []
        for (let i = 0; i < groups.length; ++i) {
            out.push({ x: col[i] * (Tiles.groupWidth + Tiles.groupGap), y: fill[col[i]] })
            fill[col[i]] += heights[i]
        }
        return out
    }
    readonly property int groupsBottom: {
        let b = 0
        for (let i = 0; i < places.length; ++i)
            b = Math.max(b, places[i].y + heights[i])
        return b
    }
    // every tile, in reading order, for the keys
    readonly property var order: {
        const out = []
        for (let i = 0; i < groups.length; ++i)
            for (let j = 0; j < groups[i].tiles.length; ++j)
                out.push(groups[i].tiles[j].id)
        return out
    }
    property int currentIndex: 0
    property bool keysUsed: false
    // where the tile being dragged would land: { group, index }, or null
    property var dropTarget: null
    property string draggedId: ""

    function groupTitle(name) {
        if (name === "@productivity")
            return i18nd("raye_shell_start", "Productivity")
        if (name === "@explore")
            return i18nd("raye_shell_start", "Explore")
        return name
    }

    // which group and place a point of `content` is over
    function targetAt(p) {
        for (let i = 0; i < groups.length; ++i) {
            const g = places[i]
            const h = heights[i] - groupSpace
            if (p.x < g.x || p.x >= g.x + Tiles.groupWidth || p.y < g.y || p.y >= g.y + h)
                continue
            const cx = Math.floor((p.x - g.x) / pitch)
            const cy = Math.floor((p.y - g.y - headerHeight) / pitch)
            const tiles = packed[i].tiles
            for (let j = 0; j < tiles.length; ++j) {
                const t = tiles[j]
                if (cx >= t.x && cx < t.x + t.w && cy >= t.y && cy < t.y + t.h)
                    return { group: i, index: j }
            }
            return { group: i, index: tiles.length }
        }
        return { group: groups.length, index: 0 }
    }

    function dropTile(id, p) {
        const t = targetAt(p)
        dropTarget = null
        draggedId = ""
        const at = Tiles.find(groups, id)
        if (at && at.group === t.group && at.index === t.index)
            return
        edited(Tiles.moveTile(groups, id, t.group, t.index))
    }

    function showCurrent() {
        const id = order[currentIndex]
        const at = Tiles.find(groups, id)
        if (!at)
            return
        const t = packed[at.group].tiles[at.index]
        const top = places[at.group].y + headerHeight + t.y * pitch
        flick.contentY = Math.max(0, Math.min(top - 8, flick.contentHeight - flick.height))
    }

    Keys.onPressed: event => {
        const n = order.length
        if (n === 0) {
            event.accepted = false
            return
        }
        switch (event.key) {
        case Qt.Key_Left:
        case Qt.Key_Up:
            currentIndex = Math.max(0, currentIndex - 1)
            break
        case Qt.Key_Right:
        case Qt.Key_Down:
            currentIndex = Math.min(n - 1, currentIndex + 1)
            break
        case Qt.Key_Return:
        case Qt.Key_Enter:
            area.launch(order[currentIndex])
            break
        case Qt.Key_Menu: {
            const at = Tiles.find(groups, order[currentIndex])
            if (at)
                area.menuRequested(order[currentIndex], groups[at.group].tiles[at.index].size,
                                   area.mapToGlobal(area.width / 2, area.height / 3))
            break
        }
        default:
            event.accepted = false
            return
        }
        keysUsed = true
        showCurrent()
        event.accepted = true
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: area.groupsBottom + 72
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        Kirigami.WheelHandler {
            target: flick
        }

        Item {
            id: content
            width: flick.width
            height: flick.contentHeight

            Repeater {
                model: area.groups.length
                delegate: Item {
                    id: group
                    required property int index
                    readonly property var entry: area.groups[index] || { name: "", tiles: [] }
                    readonly property var pack: area.packed[index] || { tiles: [], rows: 0 }
                    objectName: "tileGroup"
                    x: area.places[index] ? area.places[index].x : 0
                    y: area.places[index] ? area.places[index].y : 0
                    width: Tiles.groupWidth
                    height: (area.heights[index] || 0) - area.groupSpace

                    // the group the dragged tile is over
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: -4
                        color: area.dropTarget && area.dropTarget.group === group.index ? ShellTokens.hoverFill : "transparent"
                    }

                    // the name: click it to change it
                    Item {
                        id: header
                        objectName: "groupHeader"
                        width: parent.width
                        height: area.headerHeight
                        property bool editing: false

                        Text {
                            objectName: "groupName"
                            anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
                            visible: !header.editing
                            text: area.groupTitle(group.entry.name) !== "" ? area.groupTitle(group.entry.name)
                                  : headerMouse.containsMouse ? i18nd("raye_shell_start", "Name group") : ""
                            color: area.groupTitle(group.entry.name) !== "" ? ShellTokens.text : ShellTokens.textSecondary
                            font: Tokens.bodyStrong
                            elide: Text.ElideRight
                        }
                        MouseArea {
                            id: headerMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            visible: !header.editing
                            onClicked: {
                                field.text = area.groupTitle(group.entry.name)
                                header.editing = true
                                field.selectAll()
                                field.forceActiveFocus()
                            }
                        }
                        Rectangle {
                            visible: header.editing
                            anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
                            height: 32
                            color: Tokens.fillInputActive
                            border.width: 1
                            border.color: Qt.alpha(ShellTokens.accent, 0.8)
                            TextInput {
                                id: field
                                objectName: "groupField"
                                anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
                                verticalAlignment: TextInput.AlignVCenter
                                color: ShellTokens.text
                                selectionColor: ShellTokens.accent
                                selectedTextColor: ShellTokens.textOnAccent
                                font: Tokens.bodyStrong
                                maximumLength: 64
                                clip: true
                                function finish(keep) {
                                    if (!header.editing)
                                        return
                                    header.editing = false
                                    if (keep && text.trim() !== area.groupTitle(group.entry.name))
                                        area.edited(Tiles.rename(area.groups, group.index, text))
                                    area.forceActiveFocus()
                                }
                                Keys.onReturnPressed: finish(true)
                                Keys.onEnterPressed: finish(true)
                                Keys.onEscapePressed: event => { finish(false); event.accepted = true }
                                onActiveFocusChanged: if (!activeFocus) finish(true)
                            }
                        }
                    }

                    Repeater {
                        model: group.pack.tiles
                        delegate: Tile {
                            required property var modelData
                            readonly property var app: area.info[modelData.id] || null
                            x: modelData.x * area.pitch
                            y: area.headerHeight + modelData.y * area.pitch
                            width: modelData.w * area.pitch - Tiles.gap
                            height: modelData.h * area.pitch - Tiles.gap
                            visible: app !== null
                            favoriteId: modelData.id
                            size: modelData.size
                            name: app ? app.name : ""
                            iconSource: app ? app.icon : ""
                            keyboardCurrent: area.activeFocus && area.keysUsed && area.order[area.currentIndex] === modelData.id
                            onLaunch: area.launch(modelData.id)
                            onMenuRequested: at => area.menuRequested(modelData.id, modelData.size, at)
                            onDragMoved: at => {
                                area.draggedId = modelData.id
                                area.dropTarget = area.targetAt(mapToItem(content, at.x, at.y))
                            }
                            onDropped: at => area.dropTile(modelData.id, mapToItem(content, at.x, at.y))
                        }
                    }
                }
            }

            // below the groups: a new group, while a tile is dragged
            Rectangle {
                objectName: "newGroupZone"
                visible: area.draggedId !== ""
                y: area.groupsBottom
                width: Tiles.groupWidth
                height: 48
                color: area.dropTarget && area.dropTarget.group === area.groups.length ? Qt.alpha(ShellTokens.accent, 0.3)
                                                                                         : ShellTokens.hoverFill
                Text {
                    anchors.centerIn: parent
                    text: i18nd("raye_shell_start", "Drop here for a new group")
                    color: ShellTokens.textSecondary
                    font: Tokens.caption
                }
            }

            Text {
                objectName: "tilesEmpty"
                visible: area.groups.length === 0
                width: Tiles.groupWidth
                y: 12
                text: i18nd("raye_shell_start", "Pin apps here: right-click an app in All apps and choose Pin to Start.")
                wrapMode: Text.Wrap
                font: Tokens.body
                color: ShellTokens.textSecondary
            }
        }
    }
}
