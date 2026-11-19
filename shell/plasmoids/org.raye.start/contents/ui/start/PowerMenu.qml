// The power menu of Start: Sleep, Shut down, Restart, "Update and ..."
// when Raye Update has updates ready, Restart to Windows when there is one.
// Shared by both looks of Start.
import QtQuick
import org.raye.shell

RayeMenu {
    id: menu
    objectName: "powerMenu"

    property var backend: null
    readonly property var sessions: backend ? backend.sessions : null
    readonly property bool updatesReady: backend ? backend.updatesReady === true : false

    RayeMenuItem {
        text: i18nd("raye_shell_start", "Sleep")
        icon.name: "system-suspend"
        visible: !menu.sessions || menu.sessions.canSuspend !== false
        height: visible ? implicitHeight : 0
        onTriggered: menu.backend.power("sleep")
    }
    RayeMenuItem {
        text: i18nd("raye_shell_start", "Update and shut down")
        icon.name: "system-shutdown"
        visible: menu.updatesReady
        height: visible ? implicitHeight : 0
        onTriggered: menu.backend.power("update-shutdown")
    }
    RayeMenuItem {
        text: i18nd("raye_shell_start", "Shut down")
        icon.name: "system-shutdown"
        enabled: !menu.sessions || menu.sessions.canShutdown !== false
        onTriggered: menu.backend.power("shutdown")
    }
    RayeMenuItem {
        text: i18nd("raye_shell_start", "Update and restart")
        icon.name: "system-reboot"
        visible: menu.updatesReady
        height: visible ? implicitHeight : 0
        onTriggered: menu.backend.power("update-restart")
    }
    RayeMenuItem {
        text: i18nd("raye_shell_start", "Restart")
        icon.name: "system-reboot"
        enabled: !menu.sessions || menu.sessions.canReboot !== false
        onTriggered: menu.backend.power("restart")
    }
    RayeMenuItem {
        objectName: "restartToWindows"
        readonly property bool wanted: menu.backend ? menu.backend.canRestartToWindows === true : false
        text: i18nd("raye_shell_start", "Restart to Windows")
        icon.name: "system-reboot"
        visible: wanted
        height: visible ? implicitHeight : 0
        onTriggered: menu.backend.restartToWindows()
    }
}
