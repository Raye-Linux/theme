// Recommended: new apps and recent files in two columns, 56 px rows with
// a 32 px icon, the name and what it is ("Recently added", where the file
// is). Empty, a short sentence says what will show up here.
import QtQuick
import org.kde.kirigami as Kirigami
import org.raye.ui
import org.raye.shell

import "../menulogic.js" as Menu

FocusScope {
    id: list

    property var backend: null
    property int count: 4
    property int currentIndex: 0
    property bool keysUsed: false
    readonly property var items: Menu.recommended(showNewApps ? apps : [], showFiles ? files : [], count,
                                                  showMostUsed ? used : [])
    readonly property int rowHeight: 56

    property var apps: []
    property var files: []
    property var used: []
    // [Start] RecentApps, Recommended and MostUsed
    property bool showNewApps: true
    property bool showFiles: true
    property bool showMostUsed: false

    signal opened()

    implicitHeight: items.length === 0 ? 56 : Math.ceil(items.length / 2) * rowHeight

    // new apps (RecentlyAddedModel) and recent files (kicker RecentUsageModel)
    Instantiator {
        id: appRows
        model: list.backend && list.count > 0 ? list.backend.newApps : null
        delegate: QtObject {
            required property var model
            readonly property var entry: ({
                kind: "app",
                name: model.name || "",
                icon: model.iconName || "",
                desktopId: model.desktopId || "",
                sub: i18nd("raye_shell_start", "Recently added")
            })
        }
        onObjectAdded: refresh.restart()
        onObjectRemoved: refresh.restart()
    }
    Instantiator {
        id: fileRows
        model: list.backend && list.count > 0 ? list.backend.recentFiles : null
        delegate: QtObject {
            required property int index
            required property var model
            readonly property var entry: ({
                kind: "file",
                name: model.display || "",
                icon: model.decoration || "",
                url: model.url ? model.url.toString() : "",
                sub: model.description || "",
                row: index
            })
        }
        onObjectAdded: refresh.restart()
        onObjectRemoved: refresh.restart()
    }
    Instantiator {
        id: usedRows
        model: list.backend && list.count > 0 && list.showMostUsed ? list.backend.mostUsedApps : null
        delegate: QtObject {
            required property int index
            required property var model
            readonly property var entry: ({
                kind: "used",
                name: model.display || "",
                icon: model.decoration || "",
                sub: i18nd("raye_shell_start", "Most used"),
                row: index
            })
        }
        onObjectAdded: refresh.restart()
        onObjectRemoved: refresh.restart()
    }
    Timer {
        id: refresh
        interval: 30
        onTriggered: {
            const u = []
            for (let k = 0; k < usedRows.count; ++k)
                if (usedRows.objectAt(k)) u.push(usedRows.objectAt(k).entry)
            list.used = u
            const a = []
            for (let i = 0; i < appRows.count; ++i)
                if (appRows.objectAt(i)) a.push(appRows.objectAt(i).entry)
            const f = []
            for (let j = 0; j < fileRows.count; ++j)
                if (fileRows.objectAt(j)) f.push(fileRows.objectAt(j).entry)
            list.apps = a
            list.files = f
        }
    }

    function open(entry) {
        if (!entry || !backend)
            return
        if (entry.kind === "app")
            backend.launch(entry.desktopId)
        else if (entry.kind === "used")
            backend.trigger(backend.mostUsedApps, entry.row)
        else
            backend.trigger(backend.recentFiles, entry.row)
    }

    Keys.onPressed: event => {
        const n = items.length
        let next = currentIndex
        if (event.key === Qt.Key_Left) next = currentIndex - 1
        else if (event.key === Qt.Key_Right) next = currentIndex + 1
        else if (event.key === Qt.Key_Up) next = currentIndex - 2
        else if (event.key === Qt.Key_Down) next = currentIndex + 2
        else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) { open(items[currentIndex]); event.accepted = true; return }
        else { event.accepted = false; return }
        if (next < 0 || next >= n) { event.accepted = false; return }
        keysUsed = true
        currentIndex = next
        event.accepted = true
    }

    Text {
        objectName: "recommendedEmpty"
        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; leftMargin: 4 }
        visible: list.items.length === 0
        text: i18nd("raye_shell_start", "Your recent files and new apps will show up here.")
        wrapMode: Text.Wrap
        font: Tokens.body
        color: ShellTokens.textSecondary
    }

    Grid {
        columns: 2
        width: parent.width
        Repeater {
            model: list.items
            delegate: Item {
                id: row
                required property int index
                required property var modelData
                objectName: "recommendedItem"
                readonly property string name: modelData.name
                width: list.width / 2
                height: list.rowHeight

                Rectangle {
                    anchors { fill: parent; margins: 2 }
                    radius: Tokens.radiusControl
                    color: rowMouse.pressed ? ShellTokens.pressFill : rowMouse.containsMouse ? ShellTokens.hoverFill : "transparent"
                    Behavior on color { ColorAnimation { duration: ShellTokens.durationFaster } }
                    FocusRing { radius: Tokens.radiusControl; shown: list.activeFocus && list.keysUsed && list.currentIndex === row.index }
                }
                Kirigami.Icon {
                    id: rowIcon
                    anchors { left: parent.left; leftMargin: 14; verticalCenter: parent.verticalCenter }
                    width: 32
                    height: 32
                    source: row.modelData.icon
                }
                Column {
                    anchors { left: rowIcon.right; leftMargin: 14; right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
                    spacing: 2
                    Text {
                        width: parent.width
                        text: row.modelData.name
                        font: Tokens.body
                        color: ShellTokens.text
                        elide: Text.ElideRight
                        textFormat: Text.PlainText
                    }
                    Text {
                        width: parent.width
                        text: row.modelData.sub
                        font: Tokens.caption
                        color: ShellTokens.textSecondary
                        elide: Text.ElideMiddle
                        textFormat: Text.PlainText
                        visible: text !== ""
                    }
                }
                MouseArea {
                    id: rowMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: list.open(row.modelData)
                }
            }
        }
    }
}
