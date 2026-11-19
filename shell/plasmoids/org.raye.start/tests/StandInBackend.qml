// Backend.qml's names, with fixed data, for the tests and the harness.
// What the views ask of it lands in `calls`.
import QtQuick

QtObject {
    id: backend

    property var calls: []
    function record(what) { calls = calls.concat([what]) }

    property var pins: ["org.kde.dolphin.desktop", "firefox.desktop", "org.kde.konsole.desktop"]
    property ListModel pinnedModel: ListModel {
        ListElement { display: "Files"; decoration: "system-file-manager"; favoriteId: "org.kde.dolphin.desktop" }
        ListElement { display: "Firefox"; decoration: "firefox"; favoriteId: "firefox.desktop" }
        ListElement { display: "Konsole"; decoration: "utilities-terminal"; favoriteId: "org.kde.konsole.desktop" }
    }
    property var apps: [
        { name: "Ark", icon: "ark", favoriteId: "org.kde.ark.desktop", row: 0 },
        { name: "Calculator", icon: "accessories-calculator", favoriteId: "org.kde.kcalc.desktop", row: 1 },
        { name: "Files", icon: "system-file-manager", favoriteId: "org.kde.dolphin.desktop", row: 2 },
        { name: "Konsole", icon: "utilities-terminal", favoriteId: "org.kde.konsole.desktop", row: 3 }
    ]
    property ListModel newApps: ListModel {
        ListElement { name: "Kdenlive"; iconName: "kdenlive"; desktopId: "org.kde.kdenlive.desktop" }
    }
    property ListModel recentFiles: ListModel {
        ListElement { display: "Budget.ods"; decoration: "x-office-spreadsheet"; url: "file:///home/t/Budget.ods"; description: "Documents" }
        ListElement { display: "Trip.jpg"; decoration: "image-jpeg"; url: "file:///home/t/Trip.jpg"; description: "Pictures" }
    }
    // the most used apps, shown when [Start] MostUsed is on
    property ListModel mostUsedApps: ListModel {
        ListElement { display: "Kate"; decoration: "kate"; favoriteId: "org.kde.kate.desktop" }
    }
    // the apps by Raye category
    property bool categoriesWanted: false
    property var categories: [
        { name: "Productivity", apps: [
            { name: "Ark", icon: "ark", favoriteId: "org.kde.ark.desktop", row: 0 },
            { name: "Calculator", icon: "accessories-calculator", favoriteId: "org.kde.kcalc.desktop", row: 1 },
            { name: "Files", icon: "system-file-manager", favoriteId: "org.kde.dolphin.desktop", row: 2 },
            { name: "Konsole", icon: "utilities-terminal", favoriteId: "org.kde.konsole.desktop", row: 3 },
            { name: "Kate", icon: "kate", favoriteId: "org.kde.kate.desktop", row: 4 }
        ] },
        { name: "System", apps: [
            { name: "Konsole", icon: "utilities-terminal", favoriteId: "org.kde.konsole.desktop", row: 0 }
        ] }
    ]
    // a search that answers after `searchAnswerMs`, about what KRunner
    // takes for apps on a real system; off unless a test asks for it
    property bool fakeSearch: false
    property int searchAnswerMs: 40
    property var runner: fakeSearch ? searchGroups : null
    property var runnerIds: fakeSearch ? ["krunner_services"] : []
    property ListModel searchGroups: ListModel {
        property string query: ""
        property bool querying: answer.running
        function modelForRow(i) { return backend.searchMatches }
        onQueryChanged: answer.restart()
    }
    property ListModel searchMatches: ListModel {}
    property Timer answer: Timer {
        interval: backend.searchAnswerMs
        onTriggered: {
            backend.searchMatches.clear()
            const q = backend.searchGroups.query.toLowerCase()
            for (const a of backend.apps) {
                if (q !== "" && a.name.toLowerCase().includes(q))
                    backend.searchMatches.append({ display: a.name, decoration: a.icon, description: "",
                                                   url: "", favoriteId: a.favoriteId })
            }
            if (backend.searchGroups.count === 0)
                backend.searchGroups.append({ group: "apps" })
        }
    }
    property QtObject user: QtObject {
        property string fullName: "Test Raye"
        property string loginName: "testr"
        property url faceIconUrl: ""
    }
    property QtObject sessions: QtObject {
        property bool canSuspend: true
        property bool canShutdown: true
        property bool canReboot: true
        property bool canLogout: true
        property bool canLock: true
        property bool canSwitchUser: false
    }
    property bool updatesReady: false
    property bool appsWanted: false

    function isPinned(id) { return pins.indexOf(id) >= 0 }
    function pin(id) { record("pin " + id) }
    function unpin(id) { record("unpin " + id) }
    function moveToFront(id) { record("front " + id) }
    function movePin(from, to) { record("move " + from + " " + to) }
    function setTiles(text) { record("tiles " + text) }
    function setTileColumns(n) { record("columns " + n) }
    function isOnTaskbar(id) { return false }
    function setOnTaskbar(id, on) { record("taskbar " + id + " " + on) }
    function uninstall(id) { record("uninstall " + id) }
    function appSettings(id) { record("settings " + id) }
    function launch(id) { record("launch " + id) }
    function trigger(model, row) { record("trigger " + (model === mostUsedApps ? "mostUsed " : "") + row) }
    function openFolder(key) { record("folder " + key) }
    property bool canRestartToWindows: false
    function restartToWindows() { record("restart to windows") }
    function launchApp(entry) { record("app " + entry.favoriteId) }
    function openUrl(url) { record("url " + url) }
    function power(action) { record("power " + action) }
}
