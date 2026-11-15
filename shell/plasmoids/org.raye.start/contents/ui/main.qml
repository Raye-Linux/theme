// org.raye.start: the left part of the Raye taskbar group, the Raye eye,
// Search and Task view, and the Start popup they open. It provides
// org.kde.plasma.launchermenu, so the lone Win key opens Start.
//
// The applet leaves room before its buttons so Start, Search, Task view
// and the apps (org.raye.taskbar, right after it) are centred on the
// screen together: it reads the width of the taskbar's row from that
// applet's configuration (rowWidth) and moves itself.
import QtQuick
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import org.raye.shell

import "startlogic.js" as Logic
import "menulogic.js" as Menu
import "registry.js" as Registry

PlasmoidItem {
    id: root

    preferredRepresentation: compactRepresentation
    // the popup is this applet's own window, placed in the middle of the screen
    activationTogglesExpanded: false
    Plasmoid.backgroundHints: PlasmaCore.Types.NoBackground
    Plasmoid.status: popup.visible ? PlasmaCore.Types.RequiresAttentionStatus : PlasmaCore.Types.ActiveStatus
    Plasmoid.icon: "raye-eye"
    toolTipMainText: ""
    toolTipSubText: ""

    readonly property var screenRect: Plasmoid.containment ? Plasmoid.containment.screenGeometry : Qt.rect(0, 0, 1280, 800)

    readonly property var taskbarApplet: {
        const applets = Plasmoid.containment ? Plasmoid.containment.applets : []
        for (let i = 0; i < applets.length; ++i) {
            if (applets[i] && applets[i].pluginName === "org.raye.taskbar")
                return applets[i]
        }
        return null
    }
    readonly property int appsWidth: taskbarApplet && taskbarApplet.configuration
                                     ? (taskbarApplet.configuration.rowWidth || 0) : 0
    onAppsWidthChanged: layoutTimer.restart()
    onScreenRectChanged: layoutTimer.restart()

    // The menu's content (models, search, account, power) is made a while
    // after login, a little at a time, and kept: Start then opens with
    // everything on it, the first time like the next ones.
    property bool menuWanted: false
    // the backend is being made in the background
    property bool warming: false
    Timer {
        id: warmTimer
        interval: 10000
        onTriggered: {
            frame.prepare()
            if (root.menuWanted)
                return
            root.warming = true
            root.menuWanted = true
        }
    }
    // the list of every app, for All, once the rest is made
    Timer {
        id: listsTimer
        interval: 2000
        onTriggered: {
            if (backendLoader.item) {
                backendLoader.item.appsWanted = true
                backendLoader.item.categoriesWanted = true
            }
        }
    }
    Loader {
        id: backendLoader
        active: root.menuWanted
        asynchronous: root.warming
        source: "Backend.qml"
        onLoaded: {
            if (root.warming)
                listsTimer.start()
            root.warming = false
            item.applet = root
            item.done.connect(root.closePopup)
        }
    }

    // room around the panel for its shadow; at the bottom it is the gap
    // above the taskbar, so the panel rises from behind the taskbar's edge
    readonly property int popupMargin: Logic.flyoutGap

    function openPopup(mode, text) {
        // finishes at once whatever is still being made in the background
        root.warming = false
        root.menuWanted = true
        const row = root.compactRepresentationItem
        const panelTop = row ? row.mapToGlobal(0, 0).y : screenRect.y + screenRect.height - ShellTokens.taskbarHeight
        const panelHeight = row ? row.height : ShellTokens.taskbarHeight
        frame.screenWidth = screenRect.width
        frame.screenHeight = screenRect.height
        frame.panelHeight = panelHeight
        const pos = Logic.popupPosition(ShellSettings.alignment, screenRect, panelTop, frame.width, frame.height)
        const startX = row && row.startButton ? row.startButton.mapToGlobal(0, 0).x : screenRect.x
        popup.x = Menu.popupX(ShellSettings.alignment, screenRect.x, screenRect.width, startX, frame.width) - popupMargin
        popup.y = pos.y - popupMargin
        frame.reset(mode, text)
        frame.present()
        popup.visible = true
        popup.requestActivate()
    }

    function closePopup() {
        if (!popup.visible)
            return
        if (frame.presenting) {
            frame.presenting = false
            popup.visible = false
        }
        frame.shown = false
    }

    Component.onCompleted: {
        Registry.add(root)
        warmTimer.start()
    }
    Component.onDestruction: Registry.remove(root)

    // a global shortcut, for the Start used last
    function shortcut(name, mode) {
        const start = Registry.target(name, Date.now(), ActiveOutput.geometry())
        if (!start)
            return
        if (mode === "quicklinks")
            start.openQuickLinks()
        else
            start.toggle(mode)
    }

    function toggle(mode) {
        Registry.used(root)
        const open = popup.visible && (frame.shown || frame.presenting)
        if (open && frame.mode === mode) {
            closePopup()
        } else if (open) {
            frame.reset(mode, "")
        } else if (popup.visible) {
            // pressed again while it slides away: it comes back up
            frame.reset(mode, "")
            frame.shown = true
            popup.requestActivate()
        } else {
            openPopup(mode, "")
        }
    }

    // the lone Win key
    Connections {
        target: Plasmoid
        function onActivated() {
            root.toggle("start")
        }
    }

    GlobalShortcut {
        name: "raye-search"
        text: i18nd("raye_shell_start", "Open Search")
        sequence: "Meta+S"
        onActivated: root.shortcut("raye-search", "search")
    }
    GlobalShortcut {
        name: "raye-search-q"
        text: i18nd("raye_shell_start", "Open Search (second shortcut)")
        sequence: "Meta+Q"
        onActivated: root.shortcut("raye-search", "search")
    }
    GlobalShortcut {
        name: "raye-quick-links"
        text: i18nd("raye_shell_start", "Open the quick link menu")
        sequence: "Meta+X"
        onActivated: root.shortcut("raye-quick-links", "quicklinks")
    }
    GlobalShortcut {
        name: "raye-start"
        text: i18nd("raye_shell_start", "Open Start")
        sequence: "Ctrl+Esc"
        onActivated: root.shortcut("raye-start", "start")
    }

    function openQuickLinks() {
        Registry.used(root)
        closePopup()
        const row = root.compactRepresentationItem
        if (!row)
            return
        quickLinks.active = true
        if (!quickLinks.item)
            return
        if (quickLinks.item.opened)
            quickLinks.item.close()
        else
            quickLinks.item.open(row.startButton)
    }

    // what the quick link menu does; shutting down goes through the same
    // code as Start's power menu, made only when that menu is opened
    QtObject {
        id: quickLinkActions
        function launch(id) { Launcher.launch(id) || console.warn("quick link: cannot start", id) }
        function settings(page) { Launcher.openSettings(page) }
        function search() { root.toggle("search") }
        function desktop() { Launcher.invokeShortcut("kwin", "Show Desktop") }
        function sessions() {
            root.warming = false
            root.menuWanted = true
            return backendLoader.item ? backendLoader.item.sessions : null
        }
        function power(action) {
            if (backendLoader.item)
                backendLoader.item.power(action)
        }
    }

    Timer {
        id: layoutTimer
        interval: 50
        onTriggered: root.relayout()
    }

    function relayout() {
        const row = root.compactRepresentationItem
        if (!row || !row.Window.window)
            return
        if (!root.taskbarApplet) {
            row.leadingSpace = 0
            return
        }
        // where the applet starts on its screen, without the room it leaves
        const startX = row.mapToGlobal(0, 0).x - screenRect.x
        row.leadingSpace = Logic.leadingSpace(ShellSettings.alignment, screenRect.width, startX,
                                              row.rowWidth, root.appsWidth)
    }

    Connections {
        target: ShellSettings
        function onTaskbarChanged() { layoutTimer.restart() }
    }

    // The popup is this applet's own window (below), so the full
    // representation is empty. It must exist: without one, Plasma takes
    // the PlasmoidItem itself as the full representation and shows that
    // empty item in the panel instead of the row of buttons.
    fullRepresentation: Item {}
    // The Win key also asks Plasma to expand the applet, which would show
    // that empty item in a popup of its own over Start: never expanded.
    onExpandedChanged: if (expanded) Qt.callLater(() => { root.expanded = false })

    compactRepresentation: StartRow {
        id: startRow
        Layout.minimumWidth: implicitWidth
        Layout.preferredWidth: implicitWidth
        Layout.maximumWidth: implicitWidth
        Layout.fillHeight: true

        expanded: popup.visible
        mode: frame.mode

        onStartClicked: root.toggle("start")
        onSearchClicked: root.toggle("search")
        onTaskViewClicked: {
            root.closePopup()
            Launcher.invokeShortcut("kwin", "Overview")
        }
        onTaskViewHovered: hovered => {
            if (hovered) {
                taskViewTimer.restart()
            } else {
                taskViewTimer.stop()
                if (taskView.item)
                    taskView.item.hovering = false
            }
        }
        onQuickLinksRequested: root.openQuickLinks()
        quickLinksOpen: quickLinks.item ? quickLinks.item.opened : false
        onRowWidthChanged: layoutTimer.restart()
        onWidthChanged: layoutTimer.restart()
        Component.onCompleted: layoutTimer.restart()
        Connections {
            target: startRow.Window.window
            function onWidthChanged() { layoutTimer.restart() }
            function onXChanged() { layoutTimer.restart() }
        }
    }

    PlasmaCore.Dialog {
        id: popup
        location: PlasmaCore.Types.Floating
        type: PlasmaCore.Dialog.AppletPopup
        // the panel draws its own acrylic and shadow, so they slide with it
        backgroundHints: PlasmaCore.Dialog.NoBackground
        hideOnWindowDeactivate: true
        visible: false
        onVisibleChanged: {
            if (!visible) {
                frame.presenting = false
                frame.shown = false
            }
        }

        mainItem: FocusScope {
            width: frame.width + 2 * root.popupMargin
            height: frame.height + 2 * root.popupMargin
            focus: true

            // a click in the room around the panel closes Start
            MouseArea {
                anchors.fill: parent
                onClicked: mouse => {
                    if (!frame.contains(frame.mapFromItem(parent, mouse.x, mouse.y)))
                        root.closePopup()
                }
            }
            StartFrame {
                id: frame
                x: root.popupMargin
                y: root.popupMargin
                focus: true
                backend: backendLoader.item
                onRequestClose: root.closePopup()
                onClosed: popup.visible = false
            }
        }
    }

    // the desktops flyout over the Task view button, after 400 ms
    Timer {
        id: taskViewTimer
        interval: 400
        onTriggered: {
            taskView.active = true
            if (taskView.item) {
                taskView.item.anchorItem = root.compactRepresentationItem ? root.compactRepresentationItem.taskViewButton : null
                taskView.item.hovering = true
            }
        }
    }
    Loader {
        id: taskView
        active: false
        source: "TaskViewFlyout.qml"
    }

    Loader {
        id: quickLinks
        active: false
        source: "QuickLinkMenu.qml"
        onLoaded: item.actions = quickLinkActions
    }
}
