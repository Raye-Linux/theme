// Power flyout: Sleep, Shut down, Restart; Hibernate only when the
// backend says it works; "Update and shut down" / "Update and restart"
// when updates wait. Each row acts at once, like Windows.
import QtQuick

Flyout {
    id: power

    property bool canSleep: true
    property bool canHibernate: false
    property bool canShutDown: true
    property bool canRestart: true
    property bool updatePending: false

    signal sleepRequested()
    signal hibernateRequested()
    signal shutDownRequested()
    signal restartRequested()
    signal updateAndShutDownRequested()
    signal updateAndRestartRequested()

    rows: {
        var r = []
        if (canSleep)
            r.push({ id: "sleep", text: Strings.tr.sleep, icon: "sleep" })
        if (canHibernate)
            r.push({ id: "hibernate", text: Strings.tr.hibernate, icon: "sleep" })
        if (canShutDown && updatePending)
            r.push({ id: "updateShutDown", text: Strings.tr.updateAndShutDown, icon: "power" })
        if (canShutDown)
            r.push({ id: "shutDown", text: Strings.tr.shutDown, icon: "power" })
        if (canRestart && updatePending)
            r.push({ id: "updateRestart", text: Strings.tr.updateAndRestart, icon: "restart" })
        if (canRestart)
            r.push({ id: "restart", text: Strings.tr.restart, icon: "restart" })
        return r
    }

    onTriggered: id => {
        switch (id) {
        case "sleep": sleepRequested(); break
        case "hibernate": hibernateRequested(); break
        case "updateShutDown": updateAndShutDownRequested(); break
        case "shutDown": shutDownRequested(); break
        case "updateRestart": updateAndRestartRequested(); break
        case "restart": restartRequested(); break
        }
    }
}
