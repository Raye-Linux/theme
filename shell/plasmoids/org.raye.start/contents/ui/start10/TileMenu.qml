// The menu of a tile: unpin, resize (small, medium, wide, large), the
// taskbar, app settings, uninstall.
import QtQuick
import org.raye.shell

RayeMenu {
    id: menu

    property var backend: null
    property string appId: ""
    property string size: "medium"
    readonly property bool onTaskbar: backend && appId !== "" ? backend.isOnTaskbar(appId) : false
    readonly property bool isApp: appId.indexOf("://") < 0

    signal resize(string id, string size)

    function openFor(id, currentSize, at) {
        appId = id
        size = currentSize
        popup(at.x, at.y)
    }

    RayeMenuItem {
        objectName: "unpinTile"
        text: i18nd("raye_shell_start", "Unpin from Start")
        icon.name: "window-unpin"
        onTriggered: menu.backend.unpin(menu.appId)
    }
    RayeMenu {
        id: sizes
        title: i18nd("raye_shell_start", "Resize")
        RayeMenuItem {
            objectName: "tileSmall"
            text: i18ndc("raye_shell_start", "tile size", "Small")
            checkable: true
            checked: menu.size === "small"
            onTriggered: menu.resize(menu.appId, "small")
        }
        RayeMenuItem {
            objectName: "tileMedium"
            text: i18ndc("raye_shell_start", "tile size", "Medium")
            checkable: true
            checked: menu.size === "medium"
            onTriggered: menu.resize(menu.appId, "medium")
        }
        RayeMenuItem {
            objectName: "tileWide"
            text: i18ndc("raye_shell_start", "tile size", "Wide")
            checkable: true
            checked: menu.size === "wide"
            onTriggered: menu.resize(menu.appId, "wide")
        }
        RayeMenuItem {
            objectName: "tileLarge"
            text: i18ndc("raye_shell_start", "tile size", "Large")
            checkable: true
            checked: menu.size === "large"
            onTriggered: menu.resize(menu.appId, "large")
        }
    }
    RayeMenuItem {
        objectName: "tilePinToTaskbar"
        text: menu.onTaskbar ? i18nd("raye_shell_start", "Unpin from taskbar") : i18nd("raye_shell_start", "Pin to taskbar")
        icon.name: "pin"
        onTriggered: menu.backend.setOnTaskbar(menu.appId, !menu.onTaskbar)
    }
    RayeMenuSeparator {}
    RayeMenuItem {
        text: i18nd("raye_shell_start", "App settings")
        icon.name: "configure"
        visible: menu.isApp
        height: visible ? implicitHeight : 0
        onTriggered: menu.backend.appSettings(menu.appId)
    }
    RayeMenuItem {
        text: i18nd("raye_shell_start", "Uninstall")
        icon.name: "edit-delete"
        visible: menu.isApp
        height: visible ? implicitHeight : 0
        onTriggered: menu.backend.uninstall(menu.appId)
    }
}
