// Not a test: the toasts over the wallpaper, above a taskbar, to look at
// under Xvfb:
//
//   QML_IMPORT_PATH=<build>/qml:theme/qml <build>/bin/clock_harness <this file> [--day] [--job] [--desktop]
//
// Three cards as they stack; --job and --desktop show a file copy and the
// desktop's own senders.
import QtQuick
import QtQuick.Window
import org.raye.ui
import org.raye.shell

import "../contents/ui/toasts"

Window {
    id: win
    width: Screen.width
    height: Screen.height
    visible: true
    color: Tokens.brandNightFrom

    readonly property var args: Qt.application.arguments
    readonly property bool day: args.indexOf("--day") >= 0
    Component.onCompleted: Tokens.forceMode = day ? "day" : "night"

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: win.day ? "#e9e2f6" : "#0e0a17" }
            GradientStop { position: 0.6; color: win.day ? "#d9ccf2" : "#1a1028" }
            GradientStop { position: 1.0; color: win.day ? "#c9b4f0" : "#2a1052" }
        }
    }
    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: ShellTokens.taskbarHeight
        color: ShellTokens.taskbarSolid
    }

    Column {
        anchors { right: parent.right; bottom: parent.bottom; bottomMargin: ShellTokens.taskbarHeight + 4 }
        spacing: -4

        Loader {
            active: win.args.indexOf("--job") < 0 && win.args.indexOf("--desktop") < 0
            sourceComponent: ToastCard {
                applicationName: "Chat"
                applicationIconName: "internet-chat"
                summary: "Maya"
                body: "Are we still on for lunch at noon? I booked the place by the river, the one with the small terrace you liked last summer, and they can only hold the table for a quarter of an hour."
                iconName: "user-identity"
                hasReplyAction: true
                replyActionLabel: i18nd("raye_shell_clock", "Reply")
                actionNames: ["later"]
                actionLabels: ["Remind me later"]
                timeoutMs: 0
            }
        }
        Loader {
            active: win.args.indexOf("--job") < 0 && win.args.indexOf("--desktop") < 0
            sourceComponent: ToastCard {
                applicationName: "Raye Update"
                applicationIconName: "raye-update"
                summary: "Updates are ready"
                body: "Restart to finish installing 12 updates."
                actionNames: ["restart", "later"]
                actionLabels: ["Restart now", "Later"]
                hasDefaultAction: true
                critical: true
                timeoutMs: 0
            }
        }
        Loader {
            active: win.args.indexOf("--job") < 0 && win.args.indexOf("--desktop") < 0
            sourceComponent: ToastCard {
                applicationName: "Firefox"
                applicationIconName: "firefox"
                summary: "Download finished"
                body: "photos-of-the-river.zip"
                timeoutMs: 0
            }
        }
        Loader {
            active: win.args.indexOf("--job") >= 0
            sourceComponent: ToastCard {
                isJob: true
                jobState: 1
                percentage: 45
                killable: true
                suspendable: true
                applicationName: "Plasma Desktop Workspace"
                applicationIconName: "plasmashell"
                desktopEntry: "org.kde.plasmashell"
                summary: "Copying 3 files"
                body: "To Pictures, 2 minutes left"
                timeoutMs: 0
            }
        }
        Loader {
            active: win.args.indexOf("--desktop") >= 0
            sourceComponent: ToastCard {
                applicationName: "Plasma Desktop Workspace"
                applicationIconName: "plasmashell"
                desktopEntry: "org.kde.plasmashell"
                notifyRcName: "plasma_workspace"
                summary: "System Notifications"
                body: "Could not read file appstream://com.discordapp.discord."
                timeoutMs: 0
            }
        }
        Loader {
            active: win.args.indexOf("--desktop") >= 0
            sourceComponent: ToastCard {
                applicationName: "Network Management"
                applicationIconName: "network-wired"
                notifyRcName: "networkmanagement"
                summary: "Wired connection 1"
                body: "Connection 'Wired connection 1' activated."
                timeoutMs: 0
            }
        }
    }
}
