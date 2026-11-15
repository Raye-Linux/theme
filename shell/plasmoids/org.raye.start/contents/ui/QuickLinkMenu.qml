// The quick link menu, on the right-click of the eye and on Win+X: a text
// menu above the Start button, its left edge on the button's left edge,
// with a key in each label (no Alt needed).
// The rows are in quicklinks.js; a row whose app is not installed is not
// shown. "Shut down or sign out" opens a second menu in the same place.
import QtQuick
import org.kde.kirigami as Kirigami
import org.raye.ui
import org.raye.shell

import "quicklinks.js" as Links

Item {
    id: links

    // launch(id), settings(page), search(), desktop(), sessions(), power(action)
    property var actions: null
    property var appInstalled: id => {
        probe.desktopId = id
        probe.reload()
        return probe.valid
    }

    readonly property bool opened: mainMenu.opened || sessionMenu.opened
    readonly property bool sessionOpened: sessionMenu.opened

    // where the menus open, kept for the second one
    property Item anchorItem: null

    signal closed()

    AppJumpList {
        id: probe
        recentLimit: 0
    }

    function label(id) {
        switch (id) {
        case "apps": return i18nd("raye_shell_start", "Installed a&pps")
        case "power": return i18nd("raye_shell_start", "P&ower options")
        case "log": return i18nd("raye_shell_start", "System &log")
        case "system": return i18nd("raye_shell_start", "S&ystem")
        case "devices": return i18nd("raye_shell_start", "De&vices")
        case "network": return i18nd("raye_shell_start", "Net&work connections")
        case "disks": return i18nd("raye_shell_start", "Dis&k management")
        case "terminal": return i18nd("raye_shell_start", "Term&inal")
        case "terminal-admin": return i18nd("raye_shell_start", "Termin&al (root)")
        case "tasks": return i18nd("raye_shell_start", "&Task manager")
        case "settings": return i18nd("raye_shell_start", "Setti&ngs")
        case "files": return i18nd("raye_shell_start", "Fil&es")
        case "search": return i18nd("raye_shell_start", "&Search")
        case "run": return i18nd("raye_shell_start", "&Run")
        case "session": return i18nd("raye_shell_start", "Sh&ut down or sign out")
        case "desktop": return i18nd("raye_shell_start", "&Desktop")
        case "back": return i18nd("raye_shell_start", "&Back")
        case "signout": return i18nd("raye_shell_start", "S&ign out")
        case "sleep": return i18nd("raye_shell_start", "S&leep")
        case "shutdown": return i18nd("raye_shell_start", "Shut d&own")
        case "restart": return i18nd("raye_shell_start", "&Restart")
        }
        return ""
    }

    function open(item) {
        anchorItem = item
        fill(mainMenu, Links.rows(Links.mainEntries, links.appInstalled, {}))
        mainMenu.open(item, 0)
    }

    function close() {
        mainMenu.close()
        sessionMenu.close()
    }

    function openSession() {
        const can = actions ? actions.sessions() : {}
        fill(sessionMenu, Links.rows(Links.sessionEntries, links.appInstalled, can || {}))
        sessionMenu.open(anchorItem, 0)
    }

    function fill(menu, rows) {
        menu.clear()
        for (let i = 0; i < rows.length; ++i) {
            const e = rows[i]
            if (e.separator) {
                menu.addItem(separatorRow.createObject(null, {}))
                continue
            }
            const row = linkRow.createObject(null, { label: label(e.id), more: e.more === true })
            row.triggered.connect(() => links.run(e))
            menu.addItem(row)
        }
        menu.addItem(keysCatcher.createObject(null, { menu: menu }))
    }

    function run(e) {
        if (e.action === "session" || e.action === "back") {
            // the menu that was chosen from closes first: it must not take
            // the new one with it when it loses the focus
            Qt.callLater(e.action === "session" ? openSession : () => open(anchorItem))
        } else if (e.action === "search") {
            actions.search()
        } else if (e.action === "desktop") {
            actions.desktop()
        } else if (e.power) {
            actions.power(e.power)
        } else if (e.app) {
            actions.launch(e.app)
        } else {
            actions.settings(e.settings)
        }
    }

    Component {
        id: separatorRow
        RayeMenuSeparator {}
    }

    Component {
        id: linkRow
        RayeMenuItem {
            id: row
            objectName: "quickLinkRow"
            property string label: ""
            property bool more: false
            readonly property string accessKey: Links.accessKey(label)
            text: Links.plainLabel(label)

            contentItem: Item {
                implicitWidth: Math.max(216, name.implicitWidth + (row.more ? arrow.width + row.spacing : 0))
                implicitHeight: name.implicitHeight
                Text {
                    id: name
                    anchors.verticalCenter: parent.verticalCenter
                    text: Links.styledLabel(row.label)
                    textFormat: Text.StyledText
                    font: ShellTokens.body
                    color: ShellTokens.text
                }
                Kirigami.Icon {
                    id: arrow
                    anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                    width: 12
                    height: 12
                    visible: row.more
                    source: "arrow-right"
                    color: ShellTokens.textSecondary
                    isMask: true
                }
            }
        }
    }

    // The menu's frame has the keyboard focus through this item, so a letter
    // reaches it before the frame's own keys (arrows, Return, Esc).
    Component {
        id: keysCatcher
        Item {
            property var menu
            focus: true
            width: 0
            height: 0
            Keys.onPressed: event => {
                const rows = parent ? parent.children : []
                let current = null
                for (let i = 0; i < rows.length; ++i) {
                    const r = rows[i]
                    if (r.objectName !== "quickLinkRow")
                        continue
                    if (r.highlighted)
                        current = r
                    if (event.text !== "" && !(event.modifiers & (Qt.ControlModifier | Qt.MetaModifier))
                            && r.accessKey === event.text.toLowerCase()) {
                        r.clicked()
                        event.accepted = true
                        return
                    }
                }
                if (event.key === Qt.Key_Right && current && current.more) {
                    current.clicked()
                    event.accepted = true
                } else if (event.key === Qt.Key_Left && menu === sessionMenu) {
                    menu.close()
                    Qt.callLater(() => links.open(links.anchorItem))
                    event.accepted = true
                } else {
                    event.accepted = false
                }
            }
        }
    }

    PanelMenu {
        id: mainMenu
        fromLeft: true
        onClosed: if (!links.opened) links.closed()
    }
    PanelMenu {
        id: sessionMenu
        fromLeft: true
        onClosed: if (!links.opened) links.closed()
    }
}
