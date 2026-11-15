// All apps, in the view chosen. List: every app A to Z, 40 px rows (24 px
// icon and the name) under letter headings; clicking a heading shows the
// letters, a letter jumps to its apps (letters without apps are dimmed).
// Grid: AppIconGrid. Category: AllCategories. Right-click an app for its
// menu.
import QtQuick
import QtQuick.Controls as QQC
import org.kde.kirigami as Kirigami
import org.raye.ui
import org.raye.shell

import "../menulogic.js" as Menu

FocusScope {
    id: view

    property var backend: null
    // [Start] AllView: "category", "grid" or "list"
    property string viewKind: "list"
    readonly property var rows: Menu.withHeaders(backend ? backend.apps : [])
    readonly property var present: Menu.lettersPresent((backend ? backend.apps : []).map(a => a.name))
    property bool lettersShown: false
    property bool keysUsed: false

    signal menuRequested(var app, point at)

    Component.onCompleted: if (backend) backend.appsWanted = true
    onBackendChanged: if (backend) backend.appsWanted = true

    function reset() {
        lettersShown = false
        list.positionViewAtBeginning()
        list.currentIndex = firstApp()
        if (categoryLoader.item)
            categoryLoader.item.reset()
        if (gridLoader.item) {
            gridLoader.item.currentIndex = 0
            gridLoader.item.positionViewAtBeginning()
        }
    }
    function firstApp() {
        for (let i = 0; i < rows.length; ++i)
            if (rows[i].app) return i
        return -1
    }
    function jump(letter) {
        lettersShown = false
        const r = Menu.headerRow(rows, letter)
        if (r >= 0) {
            list.positionViewAtIndex(r, ListView.Beginning)
            list.currentIndex = Math.min(rows.length - 1, r + 1)
        }
        list.forceActiveFocus()
    }
    function step(d) {
        let i = list.currentIndex + d
        while (i >= 0 && i < rows.length && !rows[i].app)
            i += d
        if (i >= 0 && i < rows.length) {
            keysUsed = true
            list.currentIndex = i
            list.positionViewAtIndex(i, ListView.Contain)
            return true
        }
        return false
    }

    Loader {
        id: gridLoader
        // the page leaves room around the list for its backplates and bar
        anchors { fill: parent; leftMargin: 12; rightMargin: 8 }
        active: view.viewKind === "grid"
        focus: active
        sourceComponent: AppIconGrid {
            focus: true
            apps: view.backend ? view.backend.apps : []
            onLaunch: app => view.backend.launchApp(app)
            onMenuRequested: (app, at) => view.menuRequested(app, at)
        }
    }

    Loader {
        id: categoryLoader
        anchors { fill: parent; leftMargin: 12; rightMargin: 8 }
        active: view.viewKind === "category"
        focus: active
        onActiveChanged: if (active && view.backend) view.backend.categoriesWanted = true
        Component.onCompleted: if (active && view.backend) view.backend.categoriesWanted = true
        sourceComponent: AllCategories {
            focus: true
            categories: view.backend ? view.backend.categories : []
            pins: view.backend ? view.backend.pins : []
            onLaunch: app => view.backend.launchApp(app)
            onMenuRequested: (app, at) => view.menuRequested(app, at)
        }
    }

    ListView {
        id: list
        objectName: "allAppsList"
        anchors.fill: parent
        visible: view.viewKind === "list"
        clip: true
        focus: view.viewKind === "list" && !view.lettersShown
        model: view.rows
        boundsBehavior: Flickable.StopAtBounds
        reuseItems: true
        cacheBuffer: 400
        currentIndex: -1
        highlightFollowsCurrentItem: false
        QQC.ScrollBar.vertical: QQC.ScrollBar { policy: QQC.ScrollBar.AsNeeded }

        Keys.onPressed: event => {
            if (event.key === Qt.Key_Down) event.accepted = view.step(1)
            else if (event.key === Qt.Key_Up) event.accepted = view.step(-1)
            else if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && view.rows[list.currentIndex] && view.rows[list.currentIndex].app) {
                view.backend.launchApp(view.rows[list.currentIndex].app)
                event.accepted = true
            } else if (event.key === Qt.Key_Menu && list.currentItem) {
                view.menuRequested(view.rows[list.currentIndex].app, list.currentItem.mapToGlobal(40, 20))
                event.accepted = true
            } else event.accepted = false
        }

        delegate: Item {
            id: row
            required property int index
            required property var modelData
            readonly property bool isHeader: modelData.header !== undefined
            width: ListView.view.width - 12
            height: isHeader ? 36 : 40
            objectName: isHeader ? "letterHeader" : "allAppsRow"

            Rectangle {
                anchors { fill: parent; margins: 1 }
                radius: Tokens.radiusControl
                color: rowMouse.pressed ? ShellTokens.pressFill
                                        : rowMouse.containsMouse ? ShellTokens.hoverFill : "transparent"
                FocusRing {
                    radius: Tokens.radiusControl
                    shown: list.activeFocus && view.keysUsed && list.currentIndex === row.index
                }
            }

            // a letter heading: the letter in a small square, as a button
            Text {
                visible: row.isHeader
                anchors { left: parent.left; leftMargin: 14; verticalCenter: parent.verticalCenter }
                text: row.isHeader ? row.modelData.header : ""
                font: Tokens.bodyStrong
                color: ShellTokens.accent
            }

            Kirigami.Icon {
                id: appIcon
                visible: !row.isHeader
                anchors { left: parent.left; leftMargin: 14; verticalCenter: parent.verticalCenter }
                width: 24
                height: 24
                source: row.isHeader ? "" : row.modelData.app.icon
            }
            Text {
                visible: !row.isHeader
                anchors { left: appIcon.right; leftMargin: 14; right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
                text: row.isHeader ? "" : row.modelData.app.name
                font: Tokens.body
                color: ShellTokens.text
                elide: Text.ElideRight
                textFormat: Text.PlainText
            }

            MouseArea {
                id: rowMouse
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: mouse => {
                    view.keysUsed = false
                    if (row.isHeader) {
                        view.lettersShown = true
                    } else if (mouse.button === Qt.RightButton) {
                        list.currentIndex = row.index
                        view.menuRequested(row.modelData.app, mapToGlobal(mouse.x, mouse.y))
                    } else {
                        view.backend.launchApp(row.modelData.app)
                    }
                }
            }
        }
    }

    // the letters, over the list
    Item {
        id: letters
        objectName: "letterGrid"
        anchors.fill: parent
        visible: opacity > 0
        opacity: view.lettersShown ? 1 : 0
        scale: view.lettersShown ? 1 : 1.04
        Behavior on opacity { NumberAnimation { duration: ShellTokens.durationFast; easing: Motion.pointToPoint } }
        Behavior on scale { NumberAnimation { duration: ShellTokens.durationFast; easing: Motion.entrance } }
        focus: view.lettersShown
        Keys.onEscapePressed: view.lettersShown = false

        Rectangle {
            anchors.fill: parent
            color: ShellTokens.acrylicSolid
        }
        MouseArea {
            anchors.fill: parent
            onClicked: view.lettersShown = false
        }

        Grid {
            anchors.centerIn: parent
            columns: 7
            spacing: 6
            Repeater {
                model: Menu.alphabet
                delegate: Rectangle {
                    id: cell
                    required property string modelData
                    readonly property bool has: view.present[modelData] === true
                    objectName: "letter_" + modelData
                    width: 48
                    height: 48
                    radius: Tokens.radiusControl
                    color: cellMouse.containsMouse && has ? ShellTokens.hoverFill : "transparent"
                    Text {
                        anchors.centerIn: parent
                        text: cell.modelData
                        font: Tokens.subtitle
                        color: cell.has ? ShellTokens.text : Qt.alpha(ShellTokens.text, 0.25)
                    }
                    MouseArea {
                        id: cellMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        enabled: cell.has
                        onClicked: view.jump(cell.modelData)
                    }
                }
            }
        }
    }
}
