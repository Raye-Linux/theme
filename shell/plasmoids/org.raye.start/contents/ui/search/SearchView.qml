// Search, in the same window as Start: the field where Start's box was,
// results on the left grouped by kind (best match first; apps, settings,
// files, apps to get from Raye Store), the chosen result on the right with
// what can be done with it. Up and Down choose, Enter opens. With nothing
// typed: top apps and recent searches. Emptying the field goes back to
// Start.
//
// The frame (StartFrame.qml) uses:
//   property string query            what to search for, set by the frame
//   property var backend             Backend.qml, or a stand-in in tests
//   signal requestClose()            the view is done
//   signal requestStart()            the field was emptied
import QtQuick
import QtQuick.Controls as QQC
import QtQuick.Templates as T
import org.kde.kirigami as Kirigami
import org.raye.ui
import org.raye.shell

import ".."
import "../menulogic.js" as Menu
import "../start"

FocusScope {
    id: view

    property string query: ""
    property var backend: null
    signal requestClose()
    signal requestStart()

    readonly property var runner: backend ? backend.runner : null
    // [{ group, items: [{ name, icon, sub, url, favoriteId, g, row }] }]
    property var groups: []
    // the same, one row per line: { header } or { item }
    readonly property var rows: {
        const out = []
        for (let i = 0; i < groups.length; ++i) {
            out.push({ header: groups[i].group })
            for (let j = 0; j < groups[i].items.length; ++j)
                out.push({ item: groups[i].items[j] })
        }
        return out
    }
    property int currentIndex: -1
    readonly property var current: currentIndex >= 0 && currentIndex < rows.length ? rows[currentIndex].item : null

    // "No results" waits a moment: runners answer one after the other
    property bool settled: false
    Timer {
        id: settle
        interval: 600
        onTriggered: view.settled = true
    }

    onQueryChanged: {
        settled = false
        settle.restart()
        if (field.text !== query)
            field.text = query
        if (runner)
            runner.query = query
        currentIndex = -1
    }
    onRunnerChanged: if (runner) runner.query = query

    function title(group) {
        switch (group) {
        case "best": return i18nd("raye_shell_start", "Best match")
        case "apps": return i18nd("raye_shell_start", "Apps")
        case "settings": return i18nd("raye_shell_start", "Settings")
        case "files": return i18nd("raye_shell_start", "Documents")
        case "store": return i18nd("raye_shell_start", "Get it from Raye Store")
        case "answers": return i18nd("raye_shell_start", "Calculations")
        default: return i18nd("raye_shell_start", "More results")
        }
    }

    function focusField() {
        field.forceActiveFocus()
        field.cursorPosition = field.text.length
    }

    function firstItem(from, step) {
        for (let i = from; i >= 0 && i < rows.length; i += step)
            if (rows[i].item) return i
        return -1
    }
    function kindOf(item) {
        if (!item) return ""
        const g = Menu.searchGroup(backend ? backend.runnerIds[item.g] : "")
        if (g === "apps") return "app"
        if (g === "files") return "file"
        return g
    }
    function open(item) {
        if (!item || !runner)
            return
        if (query.trim() !== "" && ShellSettings.recentSearches)
            ShellSettings.addRecentSearch(query.trim())
        backend.trigger(runner.modelForRow(item.g), item.row)
    }
    function folderOf(url) {
        const s = String(url)
        return s.substring(0, s.lastIndexOf("/") + 1)
    }

    // The matches, read out of the runner's groups (one per runner, in the
    // order of backend.runnerIds) and put together again in Start's order.
    Instantiator {
        id: groupRows
        model: view.runner
        delegate: Instantiator {
            id: inner
            required property int index
            readonly property int g: index
            model: view.runner ? view.runner.modelForRow(index) : null
            delegate: QtObject {
                required property int index
                required property var model
                readonly property var entry: ({
                    name: model.display || "",
                    icon: model.decoration || "",
                    sub: model.description || "",
                    url: model.url ? model.url.toString() : "",
                    favoriteId: model.favoriteId || "",
                    g: inner.g,
                    row: index
                })
            }
            onObjectAdded: rebuild.restart()
            onObjectRemoved: rebuild.restart()
        }
        onObjectAdded: rebuild.restart()
        onObjectRemoved: rebuild.restart()
    }
    Timer {
        id: rebuild
        // matches arrive one by one in the same moment: one rebuild for
        // all of them, before the next frame
        interval: 0
        onTriggered: {
            const groups = []
            for (let g = 0; g < groupRows.count; ++g) {
                const inner = groupRows.objectAt(g)
                const items = []
                for (let i = 0; inner && i < inner.count; ++i) {
                    const o = inner.objectAt(i)
                    if (o && o.entry.name !== "")
                        items.push(o.entry)
                }
                groups.push({ runner: view.backend.runnerIds[g], items: items })
            }
            view.groups = view.query.trim() === "" ? [] : Menu.groupResults(groups, 4)
            if (view.currentIndex < 0 || !view.rows[view.currentIndex] || !view.rows[view.currentIndex].item)
                view.currentIndex = view.firstItem(0, 1)
        }
    }

    // the field, where Start's box is
    Rectangle {
        id: box
        anchors { left: parent.left; right: parent.right; top: parent.top; leftMargin: 32; rightMargin: 32; topMargin: 24 }
        height: 36
        radius: ShellTokens.windows10 ? 0 : height / 2
        color: Tokens.fillInputActive
        border.width: 1
        border.color: Qt.alpha(ShellTokens.accent, 0.8)

        SearchGlyph {
            id: glass
            x: 14
            anchors.verticalCenter: parent.verticalCenter
            width: 16
            height: 16
            color: ShellTokens.text
        }
        T.TextField {
            id: field
            objectName: "searchField"
            anchors { left: glass.right; leftMargin: 10; right: parent.right; rightMargin: 16; top: parent.top; bottom: parent.bottom }
            verticalAlignment: TextInput.AlignVCenter
            focus: true
            color: ShellTokens.text
            selectionColor: ShellTokens.accent
            selectedTextColor: ShellTokens.textOnAccent
            font: Tokens.body
            placeholderText: i18nd("raye_shell_start", "Search for apps, settings and documents")
            placeholderTextColor: ShellTokens.textSecondary
            onTextEdited: {
                view.query = text
                if (text === "")
                    view.requestStart()
            }
            Keys.onDownPressed: {
                const n = view.firstItem(view.currentIndex + 1, 1)
                if (n >= 0) view.currentIndex = n
            }
            Keys.onUpPressed: {
                const n = view.firstItem(view.currentIndex - 1, -1)
                if (n >= 0) view.currentIndex = n
            }
            Keys.onReturnPressed: view.open(view.current)
            Keys.onEnterPressed: view.open(view.current)
        }
    }

    // nothing typed yet: top apps and recent searches
    Item {
        id: idle
        objectName: "searchIdle"
        anchors { left: parent.left; right: parent.right; top: box.bottom; bottom: parent.bottom; margins: 32; topMargin: 20 }
        visible: view.query.trim() === ""

        SectionHeader {
            id: topHeader
            width: parent.width
            title: i18nd("raye_shell_start", "Top apps")
        }
        Row {
            id: topApps
            anchors { top: topHeader.bottom; topMargin: 8 }
            Repeater {
                model: view.backend ? view.backend.pinnedModel : null
                delegate: AppTile {
                    required property int index
                    required property var model
                    visible: index < 6
                    width: visible ? 96 : 0
                    height: 84
                    name: model.display || ""
                    icon: model.decoration || ""
                    hovered: topMouse.containsMouse
                    pressed: topMouse.pressed
                    MouseArea {
                        id: topMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: view.backend.trigger(view.backend.pinnedModel, parent.index)
                    }
                }
            }
        }
        SectionHeader {
            id: recentHeader
            anchors { top: topApps.bottom; topMargin: 16 }
            width: parent.width
            visible: ShellSettings.recentSearches && ShellSettings.searchRecent.length > 0
            title: i18nd("raye_shell_start", "Recent searches")
        }
        Column {
            anchors { top: recentHeader.bottom; topMargin: 4; left: parent.left; right: parent.right }
            visible: recentHeader.visible
            Repeater {
                model: ShellSettings.searchRecent.slice(0, 6)
                delegate: Item {
                    required property string modelData
                    width: parent.width
                    height: 36
                    Rectangle {
                        anchors.fill: parent
                        radius: ShellTokens.radiusButton
                        color: recentMouse.containsMouse ? ShellTokens.hoverFill : "transparent"
                    }
                    Kirigami.Icon {
                        id: clock
                        anchors { left: parent.left; leftMargin: 12; verticalCenter: parent.verticalCenter }
                        width: 16
                        height: 16
                        source: "document-open-recent"
                        color: ShellTokens.textSecondary
                        isMask: true
                    }
                    Text {
                        anchors { left: clock.right; leftMargin: 12; verticalCenter: parent.verticalCenter; right: parent.right }
                        text: parent.modelData
                        font: Tokens.body
                        color: ShellTokens.text
                        elide: Text.ElideRight
                        textFormat: Text.PlainText
                    }
                    MouseArea {
                        id: recentMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: view.query = parent.modelData
                    }
                }
            }
        }
    }

    // results on the left, the chosen one on the right
    Item {
        id: results
        anchors { left: parent.left; right: parent.right; top: box.bottom; bottom: parent.bottom; leftMargin: 20; rightMargin: 20; topMargin: 16; bottomMargin: 16 }
        visible: !idle.visible

        Text {
            objectName: "noResults"
            anchors { left: parent.left; leftMargin: 12; top: parent.top; topMargin: 12; right: parent.right }
            visible: view.rows.length === 0 && view.settled && view.runner && !view.runner.querying
            text: i18nd("raye_shell_start", "No results for “%1”", view.query.trim())
            font: Tokens.body
            color: ShellTokens.textSecondary
            wrapMode: Text.Wrap
        }

        ListView {
            id: list
            objectName: "searchResults"
            anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
            width: Math.round(parent.width * 0.46)
            clip: true
            model: view.rows
            boundsBehavior: Flickable.StopAtBounds
            currentIndex: view.currentIndex
            onCurrentIndexChanged: if (currentIndex >= 0) positionViewAtIndex(currentIndex, ListView.Contain)
            QQC.ScrollBar.vertical: QQC.ScrollBar { policy: QQC.ScrollBar.AsNeeded }

            delegate: Item {
                id: row
                required property int index
                required property var modelData
                readonly property bool isHeader: modelData.header !== undefined
                readonly property bool best: !isHeader && index === 1 && view.rows[0].header === "best"
                width: ListView.view.width - 8
                height: isHeader ? 32 : best ? 64 : 40

                Text {
                    visible: row.isHeader
                    anchors { left: parent.left; leftMargin: 12; bottom: parent.bottom; bottomMargin: 6 }
                    text: row.isHeader ? view.title(row.modelData.header) : ""
                    font: Tokens.bodyStrong
                    color: ShellTokens.text
                }

                Rectangle {
                    visible: !row.isHeader
                    anchors { fill: parent; margins: 1 }
                    radius: ShellTokens.radiusButton
                    color: view.currentIndex === row.index ? ShellTokens.selectedFill
                                                           : rowMouse.containsMouse ? ShellTokens.hoverFill : "transparent"
                    // the chosen one has the accent mark on its left
                    Rectangle {
                        visible: view.currentIndex === row.index
                        x: 0
                        anchors.verticalCenter: parent.verticalCenter
                        width: 3
                        height: 16
                        radius: 1.5
                        color: ShellTokens.accent
                    }
                }
                Kirigami.Icon {
                    id: icon
                    visible: !row.isHeader
                    anchors { left: parent.left; leftMargin: 12; verticalCenter: parent.verticalCenter }
                    width: row.best ? 32 : 24
                    height: width
                    source: row.isHeader ? "" : row.modelData.item.icon
                }
                Column {
                    visible: !row.isHeader
                    anchors { left: icon.right; leftMargin: 12; right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
                    Text {
                        width: parent.width
                        text: row.isHeader ? "" : row.modelData.item.name
                        font: row.best ? Tokens.bodyStrong : Tokens.body
                        color: ShellTokens.text
                        elide: Text.ElideRight
                        textFormat: Text.PlainText
                    }
                    Text {
                        width: parent.width
                        visible: row.best && text !== ""
                        text: row.isHeader ? "" : row.modelData.item.sub
                        font: Tokens.caption
                        color: ShellTokens.textSecondary
                        elide: Text.ElideMiddle
                        textFormat: Text.PlainText
                    }
                }
                MouseArea {
                    id: rowMouse
                    anchors.fill: parent
                    enabled: !row.isHeader
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: mouse => {
                        view.currentIndex = row.index
                        if (mouse.button === Qt.LeftButton)
                            view.open(row.modelData.item)
                    }
                }
            }
        }

        // the card of the chosen result
        Rectangle {
            id: card
            objectName: "resultCard"
            anchors { left: list.right; leftMargin: 12; right: parent.right; top: parent.top; bottom: parent.bottom }
            visible: view.current !== null
            radius: ShellTokens.radiusFlyout
            color: Tokens.bgLayer
            border.width: 1
            border.color: Tokens.strokeCard

            readonly property var item: view.current
            readonly property string kind: view.kindOf(item)
            readonly property string appId: item ? Menu.pinId(item.favoriteId) : ""

            Column {
                anchors { left: parent.left; right: parent.right; top: parent.top; topMargin: 28 }
                spacing: 6
                Kirigami.Icon {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 64
                    height: 64
                    source: card.item ? card.item.icon : ""
                }
                Item { width: 1; height: 6 }
                Text {
                    width: parent.width - 24
                    anchors.horizontalCenter: parent.horizontalCenter
                    horizontalAlignment: Text.AlignHCenter
                    text: card.item ? card.item.name : ""
                    font: Tokens.subtitle
                    color: ShellTokens.text
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    textFormat: Text.PlainText
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: card.kind === "app" ? i18nd("raye_shell_start", "App")
                        : card.kind === "settings" ? i18nd("raye_shell_start", "Raye Settings")
                        : card.kind === "file" ? i18nd("raye_shell_start", "File")
                        : card.kind === "store" ? i18nd("raye_shell_start", "Raye Store")
                        : card.kind === "answers" ? i18nd("raye_shell_start", "Result")
                        : ""
                    font: Tokens.caption
                    color: ShellTokens.textSecondary
                }
                Item { width: 1; height: 10 }
                Rectangle { width: parent.width; height: 1; color: Tokens.strokeDivider }
                Item { width: 1; height: 4 }

                CardAction {
                    text: card.kind === "store" ? i18nd("raye_shell_start", "Get")
                        : card.kind === "answers" ? i18nd("raye_shell_start", "Copy")
                        : i18nd("raye_shell_start", "Open")
                    iconName: card.kind === "store" ? "download" : card.kind === "answers" ? "edit-copy" : "document-open"
                    onClicked: view.open(card.item)
                }
                CardAction {
                    visible: card.kind === "file"
                    text: i18nd("raye_shell_start", "Open file location")
                    iconName: "folder-open"
                    onClicked: view.backend.openUrl(view.folderOf(card.item.url))
                }
                CardAction {
                    visible: card.kind === "app" && card.appId !== ""
                    text: view.backend && view.backend.isPinned(card.appId) ? i18nd("raye_shell_start", "Unpin from Start") : i18nd("raye_shell_start", "Pin to Start")
                    iconName: "window-pin"
                    onClicked: view.backend.isPinned(card.appId) ? view.backend.unpin(card.appId) : view.backend.pin(card.appId)
                }
                CardAction {
                    visible: card.kind === "app" && card.appId !== ""
                    text: view.backend && view.backend.isOnTaskbar(card.appId) ? i18nd("raye_shell_start", "Unpin from taskbar") : i18nd("raye_shell_start", "Pin to taskbar")
                    iconName: "pin"
                    onClicked: view.backend.setOnTaskbar(card.appId, !view.backend.isOnTaskbar(card.appId))
                }
                CardAction {
                    visible: card.kind === "app" && card.appId !== ""
                    text: i18nd("raye_shell_start", "App settings")
                    iconName: "configure"
                    onClicked: view.backend.appSettings(card.appId)
                }
                CardAction {
                    visible: card.kind === "app" && card.appId !== ""
                    text: i18nd("raye_shell_start", "Uninstall")
                    iconName: "edit-delete"
                    onClicked: view.backend.uninstall(card.appId)
                }
            }
        }
    }

    component CardAction: T.AbstractButton {
        id: action
        property string iconName: ""
        x: 8
        width: parent.width - 16
        height: visible ? 36 : 0
        hoverEnabled: true
        background: Rectangle {
            radius: ShellTokens.radiusButton
            color: action.down ? ShellTokens.pressFill : action.hovered ? ShellTokens.hoverFill : "transparent"
        }
        contentItem: Row {
            spacing: 12
            leftPadding: 12
            Kirigami.Icon {
                anchors.verticalCenter: parent.verticalCenter
                width: 16
                height: 16
                source: action.iconName
                color: ShellTokens.text
                isMask: true
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: action.text
                font: Tokens.body
                color: ShellTokens.text
            }
        }
    }
}
