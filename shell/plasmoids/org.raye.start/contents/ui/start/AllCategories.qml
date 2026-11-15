// The Category view of All: a tile per Raye category with its icons and
// its name, three to a row. Up to four apps show in full; with more, three
// in full and the next four small in the fourth place, pinned apps first. A tile opens in place to show its apps;
// Back or Backspace returns to the tiles.
import QtQuick
import QtQuick.Controls as QQC
import org.kde.kirigami as Kirigami
import org.raye.ui
import org.raye.shell

import "../menulogic.js" as Menu

FocusScope {
    id: view

    property var categories: []
    // the Start pins: these apps lead their tile
    property var pins: []
    // the open category, by name: the list is rebuilt when apps come or go
    property string openName: ""
    property bool keysUsed: false
    readonly property var opened: {
        for (let i = 0; i < categories.length; ++i)
            if (categories[i].name === openName)
                return categories[i]
        return null
    }

    signal launch(var app)
    signal menuRequested(var app, point at)

    function reset() {
        openName = ""
        keysUsed = false
        tiles.currentIndex = 0
    }
    function openCategory(name) {
        openName = name
        Qt.callLater(() => { if (appsLoader.item) appsLoader.item.grid.forceActiveFocus() })
    }
    function closeCategory() {
        openName = ""
        tiles.forceActiveFocus()
    }

    GridView {
        id: tiles
        anchors.fill: parent
        visible: view.opened === null
        focus: view.opened === null
        clip: true
        model: view.categories
        cellWidth: Math.floor((width - 12) / 3)
        cellHeight: 156
        boundsBehavior: Flickable.StopAtBounds
        keyNavigationEnabled: true
        keyNavigationWraps: false
        QQC.ScrollBar.vertical: QQC.ScrollBar { policy: QQC.ScrollBar.AsNeeded }

        Keys.onPressed: event => {
            view.keysUsed = true
            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                const c = view.categories[currentIndex]
                if (c)
                    view.openCategory(c.name)
                event.accepted = true
            } else {
                event.accepted = false
            }
        }

        delegate: Item {
            id: cell
            required property int index
            required property var modelData
            readonly property string name: modelData.name
            readonly property var preview: Menu.categoryPreview(modelData.apps, view.pins)
            objectName: "categoryTile"
            width: tiles.cellWidth
            height: tiles.cellHeight

            Rectangle {
                id: plate
                anchors { fill: parent; margins: 6 }
                radius: Tokens.radiusOverlay
                color: tileMouse.pressed ? ShellTokens.pressFill
                     : tileMouse.containsMouse ? ShellTokens.hoverFill : Qt.alpha(ShellTokens.text, 0.04)
                border.width: 1
                border.color: Tokens.strokeDivider
                Behavior on color { ColorAnimation { duration: ShellTokens.durationFaster } }
                FocusRing {
                    radius: plate.radius
                    shown: view.keysUsed && tiles.activeFocus && tiles.currentIndex === cell.index
                }

                Grid {
                    anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 14 }
                    columns: 2
                    spacing: 10
                    Repeater {
                        model: cell.preview.big
                        delegate: Kirigami.Icon {
                            required property var modelData
                            objectName: "categoryIcon"
                            width: 36
                            height: 36
                            source: modelData.icon
                        }
                    }
                    // the fourth place: the next apps, small
                    Grid {
                        visible: cell.preview.small.length > 0
                        width: 36
                        height: 36
                        columns: 2
                        spacing: 4
                        Repeater {
                            model: cell.preview.small
                            delegate: Kirigami.Icon {
                                required property var modelData
                                objectName: "categoryMoreIcon"
                                width: 16
                                height: 16
                                source: modelData.icon
                            }
                        }
                    }
                }
                Text {
                    anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 8 }
                    text: cell.name
                    font: Tokens.caption
                    color: ShellTokens.text
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    textFormat: Text.PlainText
                }
            }
            MouseArea {
                id: tileMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    tiles.currentIndex = cell.index
                    view.openCategory(cell.name)
                }
            }
        }
    }

    // the open category
    Loader {
        id: appsLoader
        anchors.fill: parent
        active: view.opened !== null
        focus: active
        sourceComponent: FocusScope {
            readonly property alias grid: appGrid
            Keys.onPressed: event => {
                if (event.key === Qt.Key_Backspace) {
                    view.closeCategory()
                    event.accepted = true
                }
            }
            TextLinkButton {
                id: back
                objectName: "categoryBack"
                anchors { left: parent.left; top: parent.top; leftMargin: 4 }
                text: i18nd("raye_shell_start", "Back")
                back: true
                onClicked: view.closeCategory()
            }
            Text {
                anchors { left: back.right; leftMargin: 12; verticalCenter: back.verticalCenter }
                text: view.opened ? view.opened.name : ""
                font: Tokens.bodyStrong
                color: ShellTokens.text
                textFormat: Text.PlainText
            }
            AppIconGrid {
                id: appGrid
                anchors { left: parent.left; right: parent.right; top: back.bottom; topMargin: 8; bottom: parent.bottom }
                focus: true
                tileName: "categoryApp"
                apps: view.opened ? view.opened.apps : []
                onLaunch: app => view.launch(app)
                onMenuRequested: (app, at) => view.menuRequested(app, at)
            }
        }
    }
}
