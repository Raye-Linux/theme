// Start: the search box on top, then Pinned (with "All apps" to switch
// to the list of every app) and Recommended, the account and power at the
// bottom. Typing anywhere goes to Search (the frame does that: the box
// here is a button, the real field is Search's, at the same place).
//
// The frame (StartFrame.qml) uses:
//   signal requestClose()            the view is done (an app started)
//   signal requestSearch(string)     the search box was clicked
//   property string pendingText      text typed before the view was ready
//   property var backend             Backend.qml, or a stand-in in tests
import QtQuick
import org.raye.ui
import org.raye.shell

import ".."
import "../menulogic.js" as Menu

FocusScope {
    id: view

    signal requestClose()
    signal requestSearch(string text)
    property string pendingText: ""
    property var backend: null

    // "pinned" or "all"
    property string page: "pinned"
    property bool allWanted: false
    readonly property string layout: ShellSettings.startLayout
    readonly property int pinnedRows: Menu.pinnedRows(height, layout)
    // recent files, new apps and most used each have their own key; the
    // section goes when all three are off
    readonly property int recommendedCount: Menu.recommendedCount(height, layout,
        ShellSettings.startRecommended || ShellSettings.startRecentApps || ShellSettings.startMostUsed)
    readonly property int sidePadding: 32

    // Start opens on Pinned, page one, every time
    function reset() {
        page = "pinned"
        grid.page = 0
        grid.currentIndex = 0
        grid.keysUsed = false
        if (allLoader.item)
            allLoader.item.reset()
        grid.forceActiveFocus()
    }

    function showMenu(id, inPinned, globalPoint) {
        const p = view.mapFromGlobal(globalPoint.x, globalPoint.y)
        appMenu.openFor(id, inPinned, p)
    }

    // Tab and Shift+Tab: search, pinned, recommended, account, power
    function focusChain() {
        const chain = [searchBox]
        if (page === "pinned") {
            chain.push(grid)
            if (recommended.visible)
                chain.push(recommended)
        } else if (allLoader.item) {
            chain.push(picker.currentButton, allLoader.item)
        }
        chain.push(bottom.accountButton)
        return chain.concat(bottom.folderItems(), [bottom.powerButton])
    }
    function cycle(step) {
        const chain = focusChain()
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

    // the search box, a button: clicked, it hands over to Search
    Rectangle {
        id: searchBox
        objectName: "startSearchBox"
        anchors { left: parent.left; right: parent.right; top: parent.top
                  leftMargin: view.sidePadding; rightMargin: view.sidePadding; topMargin: 24 }
        height: 36
        radius: height / 2
        activeFocusOnTab: true
        color: Tokens.fillInputActive
        border.width: 1
        border.color: activeFocus ? Qt.alpha(ShellTokens.accent, 0.8) : Tokens.strokeControl
        Accessible.role: Accessible.EditableText
        Accessible.name: i18nd("raye_shell_start", "Search for apps, settings and documents")

        SearchGlyph {
            id: glass
            x: 14
            anchors.verticalCenter: parent.verticalCenter
            width: 16
            height: 16
            color: ShellTokens.textSecondary
        }
        Text {
            anchors { left: glass.right; leftMargin: 10; right: parent.right; rightMargin: 16; verticalCenter: parent.verticalCenter }
            text: i18nd("raye_shell_start", "Search for apps, settings and documents")
            color: ShellTokens.textSecondary
            font: Tokens.body
            elide: Text.ElideRight
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.IBeamCursor
            onClicked: view.requestSearch("")
        }
        Keys.onReturnPressed: view.requestSearch("")
    }

    Item {
        id: body
        anchors { left: parent.left; right: parent.right; top: searchBox.bottom; bottom: bottom.top
                  leftMargin: view.sidePadding; rightMargin: view.sidePadding; topMargin: 20; bottomMargin: 12 }
        clip: true

        // Pinned and Recommended
        Item {
            id: pinnedPage
            width: body.width
            height: body.height
            x: view.page === "pinned" ? 0 : -body.width * 0.25
            opacity: view.page === "pinned" ? 1 : 0
            visible: opacity > 0
            Behavior on x { NumberAnimation { duration: ShellTokens.durationNormal; easing: Motion.entrance } }
            Behavior on opacity { NumberAnimation { duration: ShellTokens.durationFast; easing: Motion.pointToPoint } }

            SectionHeader {
                id: pinnedHeader
                width: parent.width
                title: i18nd("raye_shell_start", "Pinned")
                buttonText: i18nd("raye_shell_start", "All apps")
                onButtonClicked: view.page = "all"
            }

            PinnedGrid {
                id: grid
                objectName: "pinnedGrid"
                anchors { top: pinnedHeader.bottom; topMargin: 8; left: parent.left; right: parent.right }
                model: view.backend ? view.backend.pinnedModel : null
                rows: view.pinnedRows
                focus: true
                onLaunch: row => view.backend.trigger(view.backend.pinnedModel, row)
                onMoved: (from, to) => view.backend.movePin(from, to)
                onMenuRequested: (row, favoriteId, at) => view.showMenu(favoriteId, true, at)
                Keys.onDownPressed: event => {
                    if (recommended.visible) recommended.forceActiveFocus(Qt.TabFocusReason)
                }
                Keys.onUpPressed: event => searchBox.forceActiveFocus(Qt.TabFocusReason)

                Text {
                    objectName: "pinnedEmpty"
                    anchors { left: parent.left; leftMargin: 4; top: parent.top; topMargin: 12; right: parent.right }
                    visible: grid.count === 0
                    text: i18nd("raye_shell_start", "Pin apps here: right-click an app in All apps and choose Pin to Start.")
                    wrapMode: Text.Wrap
                    font: Tokens.body
                    color: ShellTokens.textSecondary
                }
            }

            SectionHeader {
                id: recommendedHeader
                anchors { top: grid.bottom; topMargin: 16 }
                width: parent.width
                visible: view.recommendedCount > 0
                title: i18nd("raye_shell_start", "Recommended")
            }

            RecommendedList {
                id: recommended
                objectName: "recommended"
                anchors { top: recommendedHeader.bottom; topMargin: 8; left: parent.left; right: parent.right }
                visible: view.recommendedCount > 0
                backend: view.backend
                count: view.recommendedCount
                showFiles: ShellSettings.startRecommended
                showNewApps: ShellSettings.startRecentApps
                showMostUsed: ShellSettings.startMostUsed
                Keys.onUpPressed: grid.forceActiveFocus(Qt.TabFocusReason)
            }
        }

        // All apps, made the first time it is shown
        Item {
            id: allPage
            width: body.width
            height: body.height
            x: view.page === "all" ? 0 : body.width * 0.25
            opacity: view.page === "all" ? 1 : 0
            visible: opacity > 0
            Behavior on x { NumberAnimation { duration: ShellTokens.durationNormal; easing: Motion.entrance } }
            Behavior on opacity { NumberAnimation { duration: ShellTokens.durationFast; easing: Motion.pointToPoint } }

            SectionHeader {
                id: allHeader
                width: parent.width
                title: i18nd("raye_shell_start", "All apps")
                buttonText: i18nd("raye_shell_start", "Back")
                buttonBack: true
                onButtonClicked: view.page = "pinned"
            }
            ViewPicker {
                id: picker
                anchors { right: allHeader.right; rightMargin: allHeader.button.width + 12; verticalCenter: allHeader.verticalCenter }
                current: ShellSettings.startAllView
                onPicked: kind => ShellSettings.startAllView = kind
            }
            Loader {
                id: allLoader
                anchors { top: allHeader.bottom; topMargin: 8; left: parent.left; right: parent.right; bottom: parent.bottom
                          leftMargin: -12; rightMargin: -20 }
                active: view.allWanted
                sourceComponent: AllAppsView {
                    objectName: "allApps"
                    backend: view.backend
                    viewKind: ShellSettings.startAllView
                    focus: true
                    onMenuRequested: (app, at) => view.showMenu(app.favoriteId, false, at)
                }
                onLoaded: if (view.page === "all") item.forceActiveFocus()
            }
        }
    }

    onPageChanged: {
        if (page === "all")
            allWanted = true
        if (page === "all" && allLoader.item)
            allLoader.item.forceActiveFocus()
        else if (page === "pinned")
            grid.forceActiveFocus()
    }
    Keys.onPressed: event => {
        if (event.key === Qt.Key_Backspace && page === "all") {
            page = "pinned"
            event.accepted = true
        }
    }

    BottomBar {
        id: bottom
        objectName: "bottomBar"
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        backend: view.backend
    }
}
