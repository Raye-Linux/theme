// The account menu of Start: account settings in Raye Settings, Lock,
// Sign out, Switch user. Shared by both looks of Start.
import QtQuick
import org.raye.shell

RayeMenu {
    id: menu

    property var backend: null
    readonly property var sessions: backend ? backend.sessions : null

    RayeMenuItem {
        text: i18nd("raye_shell_start", "Change account settings")
        icon.name: "user-identity"
        onTriggered: menu.backend.power("account")
    }
    RayeMenuSeparator {}
    RayeMenuItem {
        text: i18nd("raye_shell_start", "Lock")
        icon.name: "system-lock-screen"
        enabled: !menu.sessions || menu.sessions.canLock !== false
        onTriggered: menu.backend.power("lock")
    }
    RayeMenuItem {
        text: i18nd("raye_shell_start", "Sign out")
        icon.name: "system-log-out"
        enabled: !menu.sessions || menu.sessions.canLogout !== false
        onTriggered: menu.backend.power("signout")
    }
    RayeMenuItem {
        objectName: "switchUser"
        text: i18nd("raye_shell_start", "Switch user")
        icon.name: "system-switch-user"
        visible: menu.sessions ? menu.sessions.canSwitchUser === true : false
        height: visible ? implicitHeight : 0
        onTriggered: menu.backend.power("switch")
    }
}
