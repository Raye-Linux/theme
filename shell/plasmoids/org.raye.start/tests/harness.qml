// Not a test: the Start applet on a taskbar, to look at under Xvfb.
//
//   QML_IMPORT_PATH=<build>/qml:theme/qml <build>/bin/start_harness [--day] [--open] [--search]
//
// Without --open: the row in the four search styles, each followed by a
// few taskbar apps and centred the way the applet centres the group.
// With --open: the Start popup over the wallpaper (--search: in Search
// mode, "wi" typed), on its own acrylic.
// With --links: the quick link menu over the eye (--links-session: its
// second menu). With [Appearance] Look=windows10 in the rayeshellrc of
// $XDG_CONFIG_HOME: everything in the Windows 10 look.
import QtQuick
import QtQuick.Window
import org.raye.ui
import org.raye.shell

import "../contents/ui"
import "../contents/ui/startlogic.js" as Logic
import "../../org.raye.taskbar/contents/ui" as Taskbar

Window {
    id: win
    width: Screen.width
    height: Screen.height
    visible: true
    color: Tokens.brandNightFrom

    readonly property var args: Qt.application.arguments
    readonly property bool day: args.indexOf("--day") >= 0
    readonly property bool open: args.indexOf("--open") >= 0 || args.indexOf("--search") >= 0
    readonly property bool links: args.indexOf("--links") >= 0 || args.indexOf("--links-session") >= 0
    Component.onCompleted: Tokens.forceMode = day ? "day" : "night"

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: win.day ? "#e9e2f6" : "#0e0a17" }
            GradientStop { position: 0.6; color: win.day ? "#d9ccf2" : "#1a1028" }
            GradientStop { position: 1.0; color: win.day ? "#c9b4f0" : "#2a1052" }
        }
    }
    // a soft eye of light on the wallpaper, so the acrylic has something to show
    Rectangle {
        width: win.width * 0.5
        height: width
        radius: width / 2
        x: win.width * 0.3
        y: win.height * 0.15
        color: Qt.alpha("#c040ff", win.day ? 0.12 : 0.18)
    }

    component Opts: QtObject {
        property string searchMode: "box"
        property bool taskView: true
        property string alignment: "center"
        property string combine: "always"
        property string smallButtons: "never"
        property bool badges: true
        property bool flashing: true
        property bool windows10: ShellSettings.windows10
    }

    component Apps: QtObject {
        property var tasks
        property int revision: 0
        function counts() { return { grouped: tasks.count, ungrouped: tasks.count } }
        function launch(i) {}
        function newInstance(i) {}
        function activate(i) {}
        function minimize(i) {}
        function cycle(i) {}
        function close(i) {}
        function openUrls(i, urls) {}
        function move(from, to) {}
        function isPinned(i) { return true }
        function togglePin(i) {}
    }

    ListModel {
        id: apps
        ListElement { display: "Files"; decoration: "org.kde.dolphin"; AppId: "a"; IsLauncher: true; IsActive: false; IsMinimized: false; IsStartup: false; IsDemandingAttention: false; ChildCount: 0; LauncherUrlWithoutIcon: "" }
        ListElement { display: "Raye Store"; decoration: "org.raye.store"; AppId: "b"; IsLauncher: true; IsActive: false; IsMinimized: false; IsStartup: false; IsDemandingAttention: false; ChildCount: 0; LauncherUrlWithoutIcon: "" }
        ListElement { display: "Settings"; decoration: "systemsettings"; AppId: "c"; IsLauncher: true; IsActive: false; IsMinimized: false; IsStartup: false; IsDemandingAttention: false; ChildCount: 0; LauncherUrlWithoutIcon: "" }
        ListElement { display: "Kate"; decoration: "org.kde.kate"; AppId: "d"; IsLauncher: false; IsActive: true; IsMinimized: false; IsStartup: false; IsDemandingAttention: false; ChildCount: 0; LauncherUrlWithoutIcon: "" }
        ListElement { display: "Terminal"; decoration: "org.kde.konsole"; AppId: "e"; IsLauncher: false; IsActive: false; IsMinimized: false; IsStartup: false; IsDemandingAttention: false; ChildCount: 0; LauncherUrlWithoutIcon: "" }
    }

    // a taskbar: the Start row, then the apps, centred together
    component Bar: Rectangle {
        id: bar
        property alias opts: start.settings
        property alias start: start
        width: win.width
        height: 48
        color: ShellTokens.taskbarSolid
        Rectangle { width: parent.width; height: 1; color: ShellTokens.stroke }

        StartRow {
            id: start
            height: parent.height
            // the apps' natural width, as the taskbar publishes it
            leadingSpace: settings.windows10 ? 0 : Logic.leadingSpace(settings.alignment, bar.width, 0, rowWidth, apps.count * 44)
        }
        Taskbar.TaskBar {
            id: tasks
            x: start.x + start.width
            width: bar.width - x - 200
            height: parent.height
            groupedWithStart: true
            model: apps
            settings: start.settings
            controller: Apps { tasks: apps }
        }
        // the right end: a clock, for scale
        Column {
            anchors { right: parent.right; rightMargin: 24; verticalCenter: parent.verticalCenter }
            Text { anchors.right: parent.right; text: "14:05"; color: ShellTokens.taskbarText; font: ShellTokens.caption }
            Text { anchors.right: parent.right; text: "2026-11-01"; color: ShellTokens.taskbarText; font: ShellTokens.caption }
        }
    }

    Column {
        visible: !win.open && !win.links
        y: 24
        spacing: 20
        Repeater {
            model: ["box", "iconlabel", "icon", "hidden"]
            Column {
                required property string modelData
                spacing: 6
                Text { x: 16; text: "Search: " + modelData + (modelData === "icon" ? ", left aligned" : ""); color: win.day ? "#1a1028" : "#b9a8d6"; font.family: "Inter"; font.pixelSize: 13 }
                Bar {
                    opts: Opts { searchMode: modelData; alignment: modelData === "icon" ? "left" : "center" }
                }
            }
        }
    }

    Bar {
        id: linksBar
        visible: win.links
        anchors.bottom: parent.bottom
        opts: Opts {}
    }
    Loader {
        active: win.links
        sourceComponent: QuickLinkMenu {
            id: linksMenu
            appInstalled: id => id !== "org.raye.taskmanager.desktop"
            actions: QtObject {
                function sessions() { return ({}) }
                function launch(id) {}
                function settings(page) {}
                function search() {}
                function desktop() {}
                function power(action) {}
            }
            Timer {
                interval: 800
                running: true
                onTriggered: {
                    linksMenu.anchorItem = linksBar.start.startButton
                    if (win.args.indexOf("--links-session") >= 0)
                        linksMenu.openSession()
                    else
                        linksMenu.open(linksMenu.anchorItem)
                }
            }
        }
    }

    Bar {
        id: bottomBar
        visible: win.open
        anchors.bottom: parent.bottom
        opts: Opts {}
        start.expanded: true
        start.mode: frame.mode
    }

    Item {
        id: popup
        visible: win.open
        readonly property var size: Qt.size(frame.width, frame.height)
        readonly property var pos: ShellSettings.windows10
            ? Logic.popupPosition10(Qt.rect(0, 0, win.width, win.height), win.height - 48, size.width, size.height)
            : Logic.popupPosition("center", Qt.rect(0, 0, win.width, win.height), win.height - 48, size.width, size.height)
        x: pos.x
        y: pos.y
        width: size.width
        height: size.height

        StartFrame {
            id: frame
            screenWidth: win.width
            screenHeight: win.height
            backend: StandInBackend {}
            Component.onCompleted: {
                reset(win.args.indexOf("--search") >= 0 ? "search" : "start",
                      win.args.indexOf("--search") >= 0 ? "wi" : "")
                present()
            }
            // --rail: the Windows 10 rail open
            Timer {
                interval: 300
                running: win.args.indexOf("--rail") >= 0
                onTriggered: frame.view.railExpanded = true
            }
            // --all: on the All page, in the view of [Start] AllView
            Timer {
                interval: 300
                running: win.args.indexOf("--all") >= 0
                onTriggered: frame.view.page = "all"
            }
        }
    }
}
