// Not a test: the taskbar's states side by side, to look at under Xvfb.
//
//   QML_IMPORT_PATH=<build>/qml:theme/qml <build>/bin/taskbar_harness [--day] [--overflow-open]
//
// Rows: the usual taskbar (pinned, running, in use, asking for attention,
// a badge, an app starting), apps not combined (labels), small buttons,
// and a narrow taskbar with the "..." overflow.
import QtQuick
import QtQuick.Window
import org.raye.ui
import org.raye.shell

import "../contents/ui"

Window {
    id: win
    width: Screen.width
    height: Screen.height
    visible: true
    color: Tokens.brandNightFrom

    readonly property bool day: Qt.application.arguments.indexOf("--day") >= 0
    Component.onCompleted: Tokens.forceMode = day ? "day" : "night"

    // a wallpaper of the Raye colours behind everything
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: win.day ? "#e9e2f6" : "#0e0a17" }
            GradientStop { position: 0.6; color: win.day ? "#d9ccf2" : "#1a1028" }
            GradientStop { position: 1.0; color: win.day ? "#c9b4f0" : "#2a1052" }
        }
    }

    component Settings: QtObject {
        property string alignment: "center"
        property string combine: "always"
        property string smallButtons: "never"
        property bool badges: true
        property bool flashing: true
    }

    component Controller: QtObject {
        property var tasks
        property int revision: 0
        function counts() {
            const apps = {}
            let grouped = 0, ungrouped = 0
            for (let i = 0; i < tasks.count; ++i) {
                const t = tasks.get(i)
                if (!apps[t.AppId]) { apps[t.AppId] = true; ++grouped }
                ungrouped += Math.max(1, t.ChildCount)
            }
            return { grouped: grouped, ungrouped: ungrouped }
        }
        function launch(i) {}
        function newInstance(i) {}
        function activate(i) {}
        function minimize(i) {}
        function cycle(i) {}
        function close(i) {}
        function openUrls(i, urls) {}
        function desktopId(i) { return "" }
        function windowsOf(i) { return [] }
        function activateWindow(i, child) { }
        function closeWindow(i, child) { }
        function endTask(i) { }
        function move(from, to) {}
        function isPinned(i) { return true }
        function togglePin(i) {}
    }

    component Strip: Item {
        id: strip
        property alias model: bar.model
        property alias settings: bar.settings
        property alias controller: bar.controller
        property alias bar: bar
        property string caption: ""
        property int barWidth: win.width
        width: win.width
        height: 48 + 28

        Text {
            x: 16
            text: strip.caption
            color: win.day ? "#1a1028" : "#b9a8d6"
            font.family: "Inter"
            font.pixelSize: 13
        }
        Rectangle {
            y: 24
            width: strip.barWidth
            height: 48
            color: ShellTokens.taskbarSolid
            Rectangle { width: parent.width; height: 1; color: ShellTokens.stroke }
            TaskBar {
                id: bar
                anchors.fill: parent
                badgeCounts: ({ "org.kde.kmail2": 3, "org.raye.store": 128 })
            }
        }
    }

    function task(name, icon, extra) {
        const t = { display: name, AppName: name, decoration: icon, AppId: icon,
                    LauncherUrlWithoutIcon: "applications:" + icon + ".desktop",
                    IsLauncher: false, IsStartup: false, IsActive: false, IsMinimized: false,
                    IsDemandingAttention: false, ChildCount: 0, IsWindow: true }
        for (const k in extra)
            t[k] = extra[k]
        return t
    }

    function fill(model, list) {
        for (let i = 0; i < list.length; ++i)
            model.append(list[i])
    }

    readonly property var usual: [
        task("Files", "org.kde.dolphin", { IsLauncher: true, IsWindow: false }),
        task("Raye Store", "org.raye.store", { IsLauncher: true, IsWindow: false }),
        task("Settings", "systemsettings", { IsLauncher: true, IsWindow: false }),
        task("Notes.txt - Kate", "org.kde.kate", { IsActive: true }),
        task("Terminal", "org.kde.konsole", {}),
        task("Mail", "org.kde.kmail2", { IsDemandingAttention: true }),
        task("Music", "org.kde.elisa", { ChildCount: 2 }),
        task("Photos", "org.kde.gwenview", { IsStartup: true })
    ]

    ListModel { id: usualModel }
    ListModel { id: labelModel }
    ListModel { id: smallModel }
    ListModel { id: manyModel }

    Column {
        y: 24
        spacing: 16

        Strip {
            caption: "Centred, pinned, running, in use, attention, badges, starting"
            model: usualModel
            settings: Settings {}
            controller: Controller { tasks: usualModel }
        }
        Strip {
            caption: "Never combine (labels)"
            model: labelModel
            settings: Settings { combine: "never" }
            controller: Controller { tasks: labelModel }
        }
        Strip {
            caption: "Small buttons, left aligned"
            model: smallModel
            settings: Settings { smallButtons: "always"; alignment: "left" }
            controller: Controller { tasks: smallModel }
        }
        Strip {
            id: narrow
            caption: "Full taskbar: the … button"
            barWidth: 520
            model: manyModel
            settings: Settings {}
            controller: Controller { tasks: manyModel }
        }
    }

    Timer {
        interval: 50
        running: true
        onTriggered: {
            fill(usualModel, win.usual)
            fill(labelModel, win.usual.slice(3))
            fill(smallModel, win.usual)
            const icons = ["org.kde.dolphin", "org.raye.store", "systemsettings", "org.kde.kate", "org.kde.konsole",
                           "org.kde.elisa", "org.kde.gwenview", "org.kde.okular", "org.kde.ark", "org.kde.kcalc",
                           "org.kde.haruna", "org.kde.spectacle", "org.kde.plasma-systemmonitor", "org.kde.partitionmanager"]
            for (let i = 0; i < icons.length; ++i)
                manyModel.append(win.task("App " + i, icons[i], { IsActive: i === 2 }))
            if (Qt.application.arguments.indexOf("--overflow-open") >= 0)
                openTimer.start()
        }
    }
    Timer {
        id: openTimer
        interval: 400
        onTriggered: narrow.bar.openOverflow()
    }
}
