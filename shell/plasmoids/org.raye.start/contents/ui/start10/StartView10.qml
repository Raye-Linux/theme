// Start in the Windows 10 look: the slim rail on the left (menu, account,
// folders, power), every app A to Z under its letter with what was added
// lately on top, and the tiles on the right. Typing goes to Search (the
// frame does that). Drag the right edge to show more columns of tiles.
//
// The frame (StartFrame.qml) uses the same names as start/StartView.qml:
//   signal requestClose(), signal requestSearch(string),
//   property string pendingText, property var backend, reset()
import QtQuick
import org.kde.kirigami as Kirigami
import org.raye.ui
import org.raye.shell

import ".."
import "../start"
import "../menulogic.js" as Menu
import "../startlogic.js" as Logic
import "../tileslogic.js" as Tiles

FocusScope {
    id: view

    signal requestClose()
    signal requestSearch(string text)
    property string pendingText: ""
    property var backend: null
    // columns of tile groups (the frame sets it from [Start] TileColumns)
    property int tileColumns: 1
    property alias railExpanded: rail.expanded

    // what the pinned model shows: the ids in its order, and their names,
    // icons and rows
    property var shownPins: []
    property var pinInfo: ({})
    readonly property var tileGroups: Tiles.groups(ShellSettings.startTiles, shownPins)
    readonly property int listWidth: Logic.w10.listWidth

    function reset() {
        rail.expanded = false
        if (list.item)
            list.item.reset()
        tiles.currentIndex = 0
        tiles.keysUsed = false
        if (list.item)
            list.item.forceActiveFocus()
    }

    function readPins() {
        const ids = []
        const info = {}
        for (let i = 0; i < pinRows.count; ++i) {
            const o = pinRows.objectAt(i)
            if (!o)
                continue
            const id = Menu.pinId(o.favoriteId)
            if (id === "" || info[id])
                continue
            ids.push(id)
            info[id] = { name: o.name, icon: o.icon, row: i }
        }
        shownPins = ids
        pinInfo = info
    }
    Instantiator {
        id: pinRows
        model: view.backend ? view.backend.pinnedModel : null
        delegate: QtObject {
            required property var model
            readonly property string favoriteId: model.favoriteId || ""
            readonly property string name: model.display || ""
            readonly property var icon: model.decoration || ""
        }
        onObjectAdded: Qt.callLater(view.readPins)
        onObjectRemoved: Qt.callLater(view.readPins)
    }

    function saveTiles(groups) {
        if (backend)
            backend.setTiles(Tiles.serialize(groups))
    }

    function showMenu(id, globalPoint) {
        const p = view.mapFromGlobal(globalPoint.x, globalPoint.y)
        appMenu.openFor(id, false, p)
    }

    // Tab and Shift+Tab: the list, the tiles, the rail
    function cycle(step) {
        const chain = [list.item, tiles].concat(rail.focusItems()).filter(i => i)
        let i = chain.findIndex(it => it.activeFocus)
        i = (i + step + chain.length) % chain.length
        chain[i].forceActiveFocus(Qt.TabFocusReason)
    }
    Keys.onTabPressed: cycle(1)
    Keys.onBacktabPressed: cycle(-1)

    AppMenu {
        id: appMenu
        backend: view.backend
    }
    TileMenu {
        id: tileMenu
        objectName: "tileMenu"
        backend: view.backend
        onResize: (id, size) => view.saveTiles(Tiles.setSize(view.tileGroups, id, size))
    }

    // the list of apps, after the rail
    Loader {
        id: list
        anchors { top: parent.top; bottom: parent.bottom; left: parent.left; leftMargin: rail.railWidth }
        width: view.listWidth
        focus: true
        sourceComponent: AllAppsView {
            objectName: "allApps"
            anchors { fill: parent; topMargin: 8; bottomMargin: 8 }
            backend: view.backend
            viewKind: "list"
            square: true
            focus: true
            listHeader: recentHeader
            onMenuRequested: (app, at) => view.showMenu(app.favoriteId, at)
        }
    }

    // above the letters: what was added lately, and the most used apps
    Component {
        id: recentHeader
        Column {
            width: ListView.view ? ListView.view.width - 12 : view.listWidth
            readonly property var newApps: view.backend && ShellSettings.startRecentApps ? view.backend.newApps : null
            readonly property var mostUsed: view.backend && ShellSettings.startMostUsed ? view.backend.mostUsedApps : null

            component Heading: Text {
                leftPadding: 14
                height: 36
                verticalAlignment: Text.AlignVCenter
                font: Tokens.bodyStrong
                color: ShellTokens.text
            }
            component AppRow: Item {
                id: r
                property string name: ""
                property var iconSource: ""
                signal activated()
                width: parent ? parent.width : 0
                height: 40
                Rectangle {
                    anchors.fill: parent
                    color: rowMouse.pressed ? ShellTokens.pressFill : rowMouse.containsMouse ? ShellTokens.hoverFill : "transparent"
                }
                Kirigami.Icon {
                    id: rowIcon
                    anchors { left: parent.left; leftMargin: 14; verticalCenter: parent.verticalCenter }
                    width: 24
                    height: 24
                    source: r.iconSource
                }
                Text {
                    anchors { left: rowIcon.right; leftMargin: 14; right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
                    text: r.name
                    font: Tokens.body
                    color: ShellTokens.text
                    elide: Text.ElideRight
                    textFormat: Text.PlainText
                }
                MouseArea {
                    id: rowMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: r.activated()
                }
            }

            Heading {
                visible: newRepeater.count > 0
                text: i18ndc("raye_shell_start", "section of Start", "Recently added")
            }
            Repeater {
                id: newRepeater
                model: parent.newApps
                delegate: AppRow {
                    required property var model
                    objectName: "recentlyAddedRow"
                    visible: index < 3
                    height: visible ? 40 : 0
                    required property int index
                    name: model.name || ""
                    iconSource: model.iconName || ""
                    onActivated: view.backend.launch(model.desktopId)
                }
            }
            Heading {
                visible: usedRepeater.count > 0
                text: i18nd("raye_shell_start", "Most used")
            }
            Repeater {
                id: usedRepeater
                model: parent.mostUsed
                delegate: AppRow {
                    required property var model
                    required property int index
                    objectName: "mostUsedRow"
                    visible: index < 5
                    height: visible ? 40 : 0
                    name: model.display || ""
                    iconSource: model.decoration || ""
                    onActivated: view.backend.trigger(view.backend.mostUsedApps, index)
                }
            }
        }
    }

    TileArea {
        id: tiles
        objectName: "tileArea"
        anchors { top: parent.top; bottom: parent.bottom; right: parent.right
                  left: list.right; leftMargin: Logic.w10.tilesLeft; rightMargin: Logic.w10.tilesRight - 8 }
        anchors.topMargin: 8
        groups: view.tileGroups
        info: view.pinInfo
        columns: view.tileColumns
        backend: view.backend
        onEdited: groups => view.saveTiles(groups)
        onLaunch: id => {
            const app = view.pinInfo[id]
            if (app)
                view.backend.trigger(view.backend.pinnedModel, app.row)
        }
        onMenuRequested: (id, size, at) => {
            const p = view.mapFromGlobal(at.x, at.y)
            tileMenu.openFor(id, size, p)
        }
    }

    // the right edge: drag it for more or fewer columns of tiles
    MouseArea {
        id: edge
        objectName: "startEdge"
        anchors { top: parent.top; bottom: parent.bottom; right: parent.right }
        width: 6
        cursorShape: Qt.SizeHorCursor
        property real pressX: 0
        property int wanted: 0
        onPressed: event => {
            pressX = mapToItem(view, event.x, 0).x
            wanted = 0
        }
        onPositionChanged: event => {
            if (!pressed)
                return
            const right = mapToItem(view, event.x, 0).x
            const area = right - rail.railWidth - view.listWidth - Logic.w10.tilesLeft - Logic.w10.tilesRight
            wanted = Tiles.columnsForWidth(area)
        }
        onReleased: {
            if (wanted > 0 && view.backend)
                view.backend.setTileColumns(wanted)
            wanted = 0
        }
    }
    // where the edge would go, while it is dragged
    Rectangle {
        visible: edge.pressed && edge.wanted > 0
        x: rail.railWidth + view.listWidth + Logic.w10.tilesLeft + Logic.w10.tilesRight
           + edge.wanted * Tiles.groupWidth + (edge.wanted - 1) * Tiles.groupGap - 2
        width: 2
        height: parent.height
        color: ShellTokens.accent
    }

    StartRail {
        id: rail
        objectName: "startRail"
        anchors { top: parent.top; bottom: parent.bottom; left: parent.left }
        backend: view.backend
        z: 5
    }

    Keys.onPressed: event => {
        if (event.key === Qt.Key_Escape && rail.expanded) {
            rail.expanded = false
            event.accepted = true
        }
    }
}
