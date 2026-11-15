// Not a test: the panel above a stand-in taskbar, to look at under Xvfb.
//
//   QML_IMPORT_PATH=<build>/qml:theme/qml <build>/bin/quicksettings_harness [--day] [--edit] [--no-player] [--desktop] [--vpn]
//                                 [--wifi] [--offline] [--no-wifi-card] [--page network|bluetooth|sound]
//                                 [--expand-wifi] [--unplug [--replug]] [--cable-name <name>]
//                                 [--menu <tile>]
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

    function has(arg) { return Qt.application.arguments.indexOf(arg) >= 0 }
    function value(arg) {
        const at = Qt.application.arguments.indexOf(arg)
        return at >= 0 ? Qt.application.arguments[at + 1] : ""
    }
    readonly property bool day: has("--day")
    Component.onCompleted: Tokens.forceMode = day ? "day" : "night"

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: win.day ? "#e9e2f6" : "#0e0a17" }
            GradientStop { position: 0.6; color: win.day ? "#d9ccf2" : "#1a1028" }
            GradientStop { position: 1.0; color: win.day ? "#f4effb" : "#2a1052" }
        }
    }

    FakeBackend {
        id: fake
        hasPlayer: !win.has("--no-player")
        hasBattery: !win.has("--desktop")
        wifiAvailable: !win.has("--desktop") && !win.has("--no-wifi-card")
        bluetoothAvailable: !win.has("--desktop")
        airplaneAvailable: !win.has("--desktop")
        brightnessAvailable: !win.has("--desktop")
        vpnAvailable: win.has("--vpn")
        networkConnections: win.has("--offline") ? [{ name: "Wired connection 1", kind: "wired", state: "", signal: 0 }]
            : win.has("--wifi") ? [
                { name: "Maison", kind: "wifi", state: "active", saved: true, secured: true, signal: 80 },
                { name: "Cafe du coin", kind: "wifi", state: "", saved: false, secured: true, askPassword: true, signal: 55 },
                { name: "Bibliotheque", kind: "wifi", state: "", saved: false, secured: false, signal: 30 },
            ] : [
                { name: win.value("--cable-name") || "Wired connection 1", kind: "wired", state: "active", saved: true, signal: 0, address: "192.168.1.24" },
                { name: "Maison", kind: "wifi", state: "", saved: true, secured: true, signal: 80 },
                { name: "Cafe du coin", kind: "wifi", state: "", saved: false, secured: true, askPassword: true, signal: 55 },
            ]
        vpnConnections: win.has("--vpn") ? [{ name: "Work", active: true }, { name: "Home lab", active: false }] : []
    }

    Rectangle {
        id: bar
        anchors.bottom: parent.bottom
        width: parent.width
        height: ShellTokens.taskbarHeight
        color: ShellTokens.taskbarSolid
        QuickPill {
            anchors.right: parent.right
            anchors.rightMargin: 96
            anchors.verticalCenter: parent.verticalCenter
            backend: fake
            selected: true
        }
    }

    QuickPanel {
        id: quick
        anchors.right: parent.right
        anchors.bottom: bar.top
        backend: fake
        editing: win.has("--edit")
        Component.onCompleted: {
            if (win.has("--vpn"))
                openPage("vpn")
            else if (win.value("--page") !== "")
                openPage(win.value("--page"))
            if (win.has("--expand-wifi"))
                setWifiExpanded(true)
            if (win.value("--select") !== "")
                selectedRow = Number(win.value("--select"))
            if (win.value("--menu") !== "")
                menuTimer.start()
        }
    }
    // --unplug: the cable comes out a moment after the panel opens;
    // --replug puts it back 1.5 s later
    property var withCable: []
    Timer {
        running: win.has("--unplug")
        interval: 1500
        onTriggered: {
            win.withCable = fake.networkConnections
            fake.networkConnections = fake.networkConnections.filter(c => c.kind !== "wired")
            if (win.has("--replug"))
                replugTimer.start()
        }
    }
    Timer {
        id: replugTimer
        interval: 1500
        onTriggered: fake.networkConnections = win.withCable
    }
    // the tile is laid out once the window shows
    Timer {
        id: menuTimer
        interval: 500
        onTriggered: {
            const find = (item, name) => {
                if (item.objectName === name)
                    return item
                for (let i = 0; i < item.children.length; ++i) {
                    const found = find(item.children[i], name)
                    if (found)
                        return found
                }
                return null
            }
            const t = find(quick, "tile-" + win.value("--menu"))
            if (t)
                quick.showTileMenu(win.value("--menu"), t, 40, 30)
        }
    }
}
