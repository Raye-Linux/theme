// The menu of an app in Start (a pin, a row of All apps, a search result):
// pin or unpin, taskbar, move to front, app settings, uninstall. Settings
// and Uninstall open Raye Settings and Raye Store.
import QtQuick
import org.raye.shell

RayeMenu {
    id: menu

    property var backend: null
    property string appId: ""
    property bool inPinned: false
    readonly property bool pinned: backend && appId !== "" ? backend.isPinned(appId) : false
    readonly property bool onTaskbar: backend && appId !== "" ? backend.isOnTaskbar(appId) : false
    readonly property bool isApp: appId.indexOf("://") < 0

    function openFor(id, pinnedView, at) {
        appId = id
        inPinned = pinnedView
        popup(at.x, at.y)
    }

    RayeMenuItem {
        objectName: "pinToStart"
        text: menu.pinned ? i18nd("raye_shell_start", "Unpin from Start") : i18nd("raye_shell_start", "Pin to Start")
        icon.name: menu.pinned ? "window-unpin" : "window-pin"
        onTriggered: menu.pinned ? menu.backend.unpin(menu.appId) : menu.backend.pin(menu.appId)
    }
    RayeMenuItem {
        objectName: "pinToTaskbar"
        text: menu.onTaskbar ? i18nd("raye_shell_start", "Unpin from taskbar") : i18nd("raye_shell_start", "Pin to taskbar")
        icon.name: "pin"
        onTriggered: menu.backend.setOnTaskbar(menu.appId, !menu.onTaskbar)
    }
    RayeMenuItem {
        objectName: "moveToFront"
        text: i18nd("raye_shell_start", "Move to front")
        icon.name: "go-first"
        visible: menu.inPinned && menu.pinned
        height: visible ? implicitHeight : 0
        onTriggered: menu.backend.moveToFront(menu.appId)
    }
    RayeMenuSeparator {}
    RayeMenuItem {
        objectName: "appSettings"
        text: i18nd("raye_shell_start", "App settings")
        icon.name: "configure"
        visible: menu.isApp
        height: visible ? implicitHeight : 0
        onTriggered: menu.backend.appSettings(menu.appId)
    }
    RayeMenuItem {
        objectName: "uninstall"
        text: i18nd("raye_shell_start", "Uninstall")
        icon.name: "edit-delete"
        visible: menu.isApp
        height: visible ? implicitHeight : 0
        onTriggered: menu.backend.uninstall(menu.appId)
    }
}
