// The Raye taskbar on one screen: Start, Search and the apps in the
// middle (the same row as Plasma's, from the Start and taskbar applets),
// quick settings and the clock on the right.
import QtQuick
import QtQuick.Window
import org.kde.layershell as LayerShell
import org.raye.hypr
import org.raye.shell

Window {
    id: panel

    property var shell
    property bool main: true
    // the workspace Hyprland shows on this screen
    property int workspaceId: 1
    readonly property Item startButton: startRow.item ? startRow.item.startButton : null
    readonly property Item taskBarItem: taskBar.item

    width: screen ? screen.width : 1280
    height: ShellTokens.taskbarHeight
    color: "transparent"
    flags: Qt.FramelessWindowHint
    visible: true

    LayerShell.Window.scope: "raye-panel"
    // layer-shell-qt places the surface on the screen under the window's
    // position: `screen` alone (and LayerShell.Window.screen, which takes a
    // QScreen, not QML's screen info) left every taskbar on the first screen
    x: screen ? screen.virtualX : 0
    y: screen ? screen.virtualY : 0
    LayerShell.Window.layer: LayerShell.Window.LayerTop
    LayerShell.Window.anchors: LayerShell.Window.AnchorBottom | LayerShell.Window.AnchorLeft
                               | LayerShell.Window.AnchorRight
    LayerShell.Window.exclusionZone: height
    LayerShell.Window.keyboardInteractivity: LayerShell.Window.KeyboardInteractivityNone

    // the acrylic: Hyprland blurs what is behind (layerrule raye-acrylic)
    Rectangle {
        anchors.fill: parent
        color: ShellTokens.taskbar
        Rectangle {
            anchors { left: parent.left; right: parent.right; top: parent.top }
            height: 1
            color: Qt.alpha("#ffffff", ShellTokens.taskbarDark ? 0.08 : 0.5)
        }
    }

    TaskController {
        id: tasks
        workspaceId: panel.workspaceId
        showPins: panel.main || ShellSettings.multiDisplayApps === "all"
        grouped: !(taskBar.item && taskBar.item.labels)
    }

    readonly property int rightWidth: right.width + ShellTokens.edgeMargin
    readonly property int groupWidth: (startRow.item ? startRow.item.width : 0) + (taskBar.item ? taskBar.item.rowWidth : 0)
    readonly property bool centred: ShellSettings.alignment !== "left" && !ShellSettings.windows10

    Row {
        id: group
        x: panel.centred ? Math.max(ShellTokens.edgeMargin, Math.round((panel.width - panel.groupWidth) / 2))
                         : (ShellSettings.windows10 ? 0 : ShellTokens.edgeMargin)
        height: parent.height
        Behavior on x { NumberAnimation { duration: ShellTokens.durationNormal; easing: Motion.pointToPoint } }

        Loader {
            id: startRow
            height: parent.height
            source: Views.url("org.raye.start", "StartRow.qml")
            onLoaded: {
                item.startClicked.connect(() => panel.shell.toggle("start", panel))
                item.searchClicked.connect(() => panel.shell.toggle("search", panel))
                item.quickLinksRequested.connect(() => panel.shell.toggle("start", panel))
            }
            Binding {
                target: startRow.item
                property: "expanded"
                value: panel.shell.startOpen
                when: startRow.item !== null
            }
            // Start or Search shows as the one that is open
            Binding {
                target: startRow.item
                property: "mode"
                value: panel.shell.startMode
                when: startRow.item !== null
            }
        }

        Loader {
            id: taskBar
            height: parent.height
            width: Math.max(ShellTokens.buttonPitch, panel.width - panel.rightWidth - group.x
                            - (startRow.item ? startRow.item.width : 0) - ShellTokens.edgeMargin)
            source: Views.url("org.raye.taskbar", "TaskBar.qml")
            onLoaded: {
                item.model = tasks.model
                item.controller = tasks
                item.groupedWithStart = true
            }
        }
    }

    Row {
        id: right
        anchors.right: parent.right
        anchors.rightMargin: ShellTokens.edgeMargin
        height: parent.height
        spacing: 2

        Loader {
            id: pill
            anchors.verticalCenter: parent.verticalCenter
            source: Views.url("org.raye.quicksettings", "QuickPill.qml")
            onLoaded: {
                item.backend = Qt.binding(() => panel.shell.system)
                item.clicked.connect(() => panel.shell.toggle("quick", panel))
            }
            Binding {
                target: pill.item
                property: "selected"
                value: panel.shell.quickOpen
                when: pill.item !== null
            }
        }

        Loader {
            id: clock
            anchors.verticalCenter: parent.verticalCenter
            source: Views.url("org.raye.clock", "ClockButton.qml")
            onLoaded: {
                item.now = Qt.binding(() => panel.shell.now)
                item.twentyFour = Qt.binding(() => panel.shell.twentyFour)
                item.showSeconds = Qt.binding(() => ShellSettings.clockShowSeconds)
                item.showDate = Qt.binding(() => ShellSettings.clockShowDate)
                item.longDate = Qt.binding(() => panel.shell.now.toLocaleDateString(Qt.locale(), Locale.LongFormat))
                item.unreadCount = Qt.binding(() => panel.shell.unreadCount)
                item.clicked.connect(() => panel.shell.toggle("centre", panel))
            }
        }
    }
}
