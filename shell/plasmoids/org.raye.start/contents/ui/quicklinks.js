// The quick link menu (right-click on the eye, Win+X): its rows, and the
// keys in their labels. Plain functions, so they can be tested without
// Plasma. The labels themselves are in QuickLinkMenu.qml, to be translated.
.pragma library

// settings: a page of Raye Settings. apps: desktop ids, the first one
// installed is the one that starts, none and the row is not there. needs:
// another app the row cannot work without. action: done by the menu.
var mainEntries = [
    { id: "apps", settings: "apps/installed" },
    { id: "power", settings: "system/power" },
    { id: "log", apps: ["org.kde.ksystemlog.desktop"] },
    { id: "system", settings: "system/about" },
    { id: "devices", settings: "devices" },
    { id: "network", settings: "network" },
    { id: "disks", apps: ["org.kde.partitionmanager.desktop"] },
    { separator: true },
    { id: "terminal", apps: ["org.kde.konsole.desktop"] },
    { id: "terminal-admin", apps: ["org.raye.terminal-admin.desktop"], needs: "org.kde.konsole.desktop" },
    { id: "tasks", apps: ["org.raye.taskmanager.desktop", "org.kde.plasma-systemmonitor.desktop"] },
    { separator: true },
    { id: "settings", settings: "" },
    { id: "files", apps: ["org.kde.dolphin.desktop"] },
    { id: "search", action: "search" },
    { id: "run", apps: ["org.raye.run.desktop"] },
    { separator: true },
    { id: "session", action: "session", more: true },
    { id: "desktop", action: "desktop" }
]

// The second menu. can: what the machine must allow for the row to show.
var sessionEntries = [
    { id: "back", action: "back" },
    { separator: true },
    { id: "signout", power: "signout", can: "canLogout" },
    { id: "sleep", power: "sleep", can: "canSuspend" },
    { id: "shutdown", power: "shutdown", can: "canShutdown" },
    { id: "restart", power: "restart", can: "canReboot" }
]

// The rows to show: entries that can work, `app` set to the one that
// starts, and no separator first, last or next to another.
// installed(desktopId) -> bool; allowed: { canLogout: true, ... }
function rows(entries, installed, allowed) {
    var out = []
    for (var i = 0; i < entries.length; ++i) {
        var e = entries[i]
        if (e.separator) {
            if (out.length > 0 && !out[out.length - 1].separator)
                out.push(e)
            continue
        }
        if (e.can && allowed[e.can] === false)
            continue
        if (e.needs && !installed(e.needs))
            continue
        var row = {}
        for (var k in e)
            row[k] = e[k]
        if (e.apps) {
            row.app = ""
            for (var a = 0; a < e.apps.length && row.app === ""; ++a) {
                if (installed(e.apps[a]))
                    row.app = e.apps[a]
            }
            if (row.app === "")
                continue
        }
        out.push(row)
    }
    while (out.length > 0 && out[out.length - 1].separator)
        out.pop()
    return out
}

// "Power &options" -> "o"; "" when the label has no key.
function accessKey(label) {
    var at = String(label).indexOf("&")
    return at >= 0 ? String(label).charAt(at + 1).toLowerCase() : ""
}

function plainLabel(label) {
    return String(label).replace("&", "")
}

// the label with its key underlined, as StyledText
function styledLabel(label) {
    var s = String(label)
    var at = s.indexOf("&")
    function escaped(t) {
        return t.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
    }
    if (at < 0)
        return escaped(s)
    return escaped(s.substring(0, at)) + "<u>" + escaped(s.charAt(at + 1)) + "</u>" + escaped(s.substring(at + 2))
}
