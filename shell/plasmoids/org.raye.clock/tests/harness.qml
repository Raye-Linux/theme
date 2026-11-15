// Not a test: the clock and the notification centre, to look at under Xvfb.
//
//   QML_IMPORT_PATH=<build>/qml:theme/qml <build>/bin/clock_harness [--day] [--dnd] [--focus] [--empty] [--collapsed] [--calendar] [--count N]
//
// The centre open over the wallpaper above a taskbar whose right end
// shows the clock, and four clocks side by side: plain, with unread
// notifications, in Do not disturb and during a Focus session.
import QtQuick
import QtQuick.Window
import org.raye.ui
import org.raye.shell

import "../contents/ui"
import "../contents/ui/clocklogic.js" as Logic

Window {
    id: win
    width: Screen.width
    height: Screen.height
    visible: true
    color: Tokens.brandNightFrom

    readonly property var args: Qt.application.arguments
    readonly property bool day: args.indexOf("--day") >= 0
    readonly property bool dnd: args.indexOf("--dnd") >= 0
    readonly property bool focusOn: args.indexOf("--focus") >= 0
    readonly property date now: new Date(2026, 10, 1, 14, 5, 9)
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
        width: win.width * 0.45
        height: width
        radius: width / 2
        x: win.width * 0.55
        y: win.height * 0.2
        color: Qt.alpha("#c040ff", win.day ? 0.12 : 0.2)
    }

    // four clocks, for comparison
    Row {
        x: 24
        y: 24
        spacing: 24
        Repeater {
            model: 4
            Rectangle {
                required property int index
                width: 160
                height: 48
                color: ShellTokens.taskbarSolid
                radius: 4
                ClockButton {
                    anchors.centerIn: parent
                    now: win.now
                    twentyFour: index !== 1
                    showSeconds: index === 3
                    unreadCount: index === 1 ? 4 : 0
                    doNotDisturb: index === 2
                    focusFraction: index === 3 ? 0.6 : -1
                    longDate: win.now.toLocaleDateString(Qt.locale(), Locale.LongFormat)
                }
            }
        }
    }

    ListModel { id: notifications }

    function fill() {
        if (win.args.indexOf("--empty") >= 0)
            return
        const minutes = m => new Date(win.now.getTime() - m * 60000)
        // --count N: that many screenshots taken, as on the owner's laptop
        const at = win.args.indexOf("--count")
        if (at >= 0) {
            for (let i = 0; i < Number(win.args[at + 1]); ++i)
                notifications.append({ isGroup: false, isInGroup: false, type: 1, applicationName: "Spectacle", applicationIconName: "org.kde.spectacle",
                                       summary: "Rectangular Region", body: "A screenshot was saved to your clipboard.", created: minutes(i + 1),
                                       actionNames: "", actionLabels: "", hasDefaultAction: true })
            return
        }
        notifications.append({ isGroup: true, isInGroup: false, type: 1, applicationName: "Chat", applicationIconName: "org.kde.konversation",
                               summary: "", body: "", created: minutes(2), actionNames: "", actionLabels: "", hasDefaultAction: false })
        notifications.append({ isGroup: false, isInGroup: true, type: 1, applicationName: "Chat", applicationIconName: "org.kde.konversation",
                               summary: "Maya", body: "Are we still on for lunch at noon? I booked the place by the river.", created: minutes(2),
                               actionNames: "reply\nlater", actionLabels: "Reply\nRemind me later", hasDefaultAction: true })
        notifications.append({ isGroup: false, isInGroup: true, type: 1, applicationName: "Chat", applicationIconName: "org.kde.konversation",
                               summary: "Leo", body: "Sent you the photos.", created: minutes(14), actionNames: "", actionLabels: "", hasDefaultAction: true })
        notifications.append({ isGroup: false, isInGroup: false, type: 2, applicationName: "Files", applicationIconName: "org.kde.dolphin",
                               summary: "Copying 3 files to Pictures", body: "", created: minutes(1), percentage: 45, jobState: 1, killable: true,
                               actionNames: "", actionLabels: "", hasDefaultAction: false })
        notifications.append({ isGroup: false, isInGroup: false, type: 1, applicationName: "Raye Update", applicationIconName: "raye-update",
                               summary: "Updates are ready", body: "Restart to finish installing 12 updates.", created: minutes(200),
                               actionNames: "restart", actionLabels: "Restart now", hasDefaultAction: true })
    }

    QtObject {
        id: fakeController
        function close(row) {}
        function clearAll() {}
        function invokeDefault(row) {}
        function invokeAction(row, id) {}
        function killJob(row) {}
        function toggleDnd() {}
        function startFocus(minutes) {}
        function stopFocus() {}
        function requestAgenda(from, to) {}
        function openCalendar(day, isNew) {}
    }

    // the taskbar, with the clock at its right end and the centre above it
    Rectangle {
        id: bar
        anchors.bottom: parent.bottom
        width: win.width
        height: 48
        color: ShellTokens.taskbarSolid
        Rectangle { width: parent.width; height: 1; color: ShellTokens.stroke }
        ClockButton {
            id: clock
            anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
            now: win.now
            selected: true
            doNotDisturb: win.dnd
            focusFraction: win.focusOn ? 0.6 : -1
        }
    }

    Centre {
        id: centre
        x: win.width - width + margin - 12
        y: bar.y - 12 - height + margin
        model: notifications
        controller: fakeController
        now: win.now
        doNotDisturb: win.dnd || win.focusOn
        focusRemaining: win.focusOn ? 18 * 60 + 24 : 0
        calendarCollapsed: win.args.indexOf("--collapsed") >= 0
        // --calendar: Raye Calendar installed, with a few events
        calendarAvailable: win.args.indexOf("--calendar") >= 0
        calendarEvents: calendarAvailable ? [
            { title: "Dentist", start: new Date(2026, 10, 1, 9, 30), end: new Date(2026, 10, 1, 10, 30), allDay: false, color: "" },
            { title: "Call with Marc", start: new Date(2026, 10, 1, 16, 0), end: new Date(2026, 10, 1, 16, 30), allDay: false, color: "#2f80ed" },
            { title: "Weekend away", start: new Date(2026, 9, 31), end: new Date(2026, 10, 2), allDay: true, color: "#d9822b" },
            { title: "Book club", start: new Date(2026, 10, 4, 19, 0), end: new Date(2026, 10, 4, 21, 0), allDay: false, color: "" },
            { title: "Remembrance Day", start: new Date(2026, 10, 11), end: new Date(2026, 10, 12), allDay: true, holiday: true, color: "" },
        ] : []
        maxHeight: bar.y - 24
        Component.onCompleted: win.fill()
    }
}
