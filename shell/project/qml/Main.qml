// The Project panel's window: the panel and the room for its shadow. It
// leaves when something else gets the keyboard.
import QtQuick
import QtQuick.Window
import org.raye.shell

Window {
    id: root

    required property QtObject controller

    width: panelItem.width
    height: panelItem.height
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: "transparent"
    title: i18nd("raye_shell_project", "Project")

    property bool hadFocus: false
    onActiveChanged: {
        if (active)
            hadFocus = true
        else if (hadFocus)
            panelItem.shown = false
    }

    ProjectPanel {
        id: panelItem
        controller: root.controller
        focus: true
        onRequestClose: shown = false
        onClosed: root.controller.quit()
        onOpenDisplaySettings: Launcher.openSettings("system/display")
    }
}
