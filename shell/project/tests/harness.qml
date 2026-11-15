// Not a test: the panel above a stand-in taskbar, to look at under Xvfb.
//   QML_IMPORT_PATH=<build>/qml:theme/qml <build>/bin/project_harness [--day] [--one] [--mode=<mode>]
import QtQuick
import QtQuick.Window
import org.raye.ui
import org.raye.shell

import "../qml"

Window {
    id: win
    width: Screen.width
    height: Screen.height
    visible: true

    readonly property var args: Qt.application.arguments
    readonly property bool day: args.indexOf("--day") >= 0
    Component.onCompleted: Tokens.forceMode = day ? "day" : "night"

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: win.day ? "#e9e2f6" : "#0e0a17" }
            GradientStop { position: 0.6; color: win.day ? "#d9ccf2" : "#1a1028" }
            GradientStop { position: 1.0; color: win.day ? "#f4effb" : "#2a1052" }
        }
    }

    FakeController {
        id: fake
        canChoose: win.args.indexOf("--one") < 0
        currentMode: {
            for (const a of win.args) {
                if (a.startsWith("--mode="))
                    return a.substring(7)
            }
            return win.args.indexOf("--one") >= 0 ? "pc-only" : "extend"
        }
    }

    Rectangle {
        id: bar
        anchors.bottom: parent.bottom
        width: parent.width
        height: ShellTokens.taskbarHeight
        color: ShellTokens.taskbarSolid
    }

    ProjectPanel {
        anchors.right: parent.right
        anchors.bottom: bar.top
        controller: fake
    }
}
