// The right-click menu of a taskbar button, as in Windows: the files the
// app opened last and its own tasks on top, then the app itself (a new
// window), "Pin to taskbar" / "Unpin from taskbar" and "Close window".
// No settings of Plasma in it: the taskbar is set up in Raye Settings.
// The taskbar loads it by name, sets the properties below, then calls
// open().
import QtQuick
import org.raye.shell

import "taskbarlogic.js" as Logic

Item {
    id: jumpList

    property var controller
    property int index: -1
    property string title: ""
    property bool isLauncher: false
    property int childCount: 0
    property Item visualParent

    property bool pinned: false
    readonly property bool opened: menu.opened

    function open() {
        if (!visualParent || !controller)
            return
        pinned = controller.isPinned(index)
        jump.desktopId = controller.desktopId(index)
        jump.reload()
        build()
        menu.open(visualParent, visualParent.width / 2)
    }

    AppJumpList {
        id: jump
        recentLimit: 8
    }

    function add(component, props) {
        const item = component.createObject(null, props)
        menu.addItem(item)
        return item
    }

    function build() {
        menu.clear()

        const entries = Logic.jumpListEntries({
            recent: jump.recent.length, actions: jump.actions.length, valid: jump.valid,
            pinned: pinned, isLauncher: isLauncher, childCount: childCount,
            endTask: ShellSettings.endTask
        })
        for (let i = 0; i < entries.length; ++i) {
            const e = entries[i]
            switch (e.kind) {
            case "header":
                add(headerItem, { text: e.section === "recent" ? i18nd("raye_shell_taskbar", "Recent")
                                                               : i18nd("raye_shell_taskbar", "Tasks") })
                break
            case "recent": {
                const r = jump.recent[e.at]
                add(actionItem, { text: r.name, "icon.name": r.icon }).triggered.connect(() => jump.openRecent(r.url))
                break
            }
            case "action": {
                const a = jump.actions[e.at]
                const at = e.at
                add(actionItem, { text: a.text, "icon.name": a.icon }).triggered.connect(() => jump.runAction(at))
                break
            }
            case "separator":
                add(separatorItem, {})
                break
            case "app":
                add(actionItem, { text: jump.name !== "" ? jump.name : title, "icon.name": jump.iconName })
                    .triggered.connect(() => controller.newInstance(index))
                break
            case "pin":
                add(actionItem, { text: i18nd("raye_shell_taskbar", "Pin to taskbar"), "icon.name": "window-pin" })
                    .triggered.connect(() => controller.togglePin(index))
                break
            case "unpin":
                add(actionItem, { text: i18nd("raye_shell_taskbar", "Unpin from taskbar"), "icon.name": "window-unpin" })
                    .triggered.connect(() => controller.togglePin(index))
                break
            case "endTask":
                add(actionItem, { text: i18nd("raye_shell_taskbar", "End task"), "icon.name": "process-stop" })
                    .triggered.connect(() => controller.endTask(index))
                break
            case "close":
                add(actionItem, { text: childCount > 1 ? i18nd("raye_shell_taskbar", "Close all windows")
                                                       : i18nd("raye_shell_taskbar", "Close window"),
                                  "icon.name": "window-close" })
                    .triggered.connect(() => controller.close(index))
                break
            }
        }
    }

    Component { id: actionItem; RayeMenuItem {} }
    Component { id: headerItem; RayeMenuItem { enabled: false; font.weight: Font.DemiBold } }
    Component { id: separatorItem; RayeMenuSeparator {} }

    PanelMenu {
        id: menu
    }
}
