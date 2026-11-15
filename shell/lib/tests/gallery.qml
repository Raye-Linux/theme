// Every piece of org.raye.shell on one page, to look at (not a test):
//
//   QML_IMPORT_PATH=<build>/qml:theme/qml qml6 shell/lib/tests/gallery.qml
//
// "qml6 gallery.qml -- --day" shows it in Raye Day.
import QtQuick
import QtQuick.Window
import org.raye.ui
import org.raye.shell

Window {
    id: root
    width: 1280
    height: 800
    visible: true
    color: Tokens.bgBase
    title: "org.raye.shell"

    Component.onCompleted: {
        if (Qt.application.arguments.indexOf("--day") >= 0)
            Tokens.forceMode = "day"
        else
            Tokens.forceMode = "night"
        menu.popup(menuAnchor, 0, 0)
    }

    Image {
        anchors.fill: parent
        source: Qt.resolvedUrl("../../../wallpaper/Raye/contents/images/1920x1080.png")
        fillMode: Image.PreserveAspectCrop
        opacity: Tokens.night ? 0.85 : 0.35
    }

    // a taskbar
    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: ShellTokens.taskbarHeight
        color: ShellTokens.taskbar
        Rectangle { width: parent.width; height: 1; color: ShellTokens.stroke }

        Row {
            anchors.centerIn: parent
            spacing: ShellTokens.buttonPitch - ShellTokens.buttonSize
            PanelButton { icon.name: "start-here"; toolTipText: "Start" }
            PanelButton { icon.name: "system-search"; toolTipText: "Search" }
            PanelButton { icon.name: "system-file-manager"; selected: true
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 1
                    width: ShellTokens.pillLong; height: ShellTokens.pillHeight; radius: height / 2
                    color: ShellTokens.accent
                }
            }
            PanelButton { icon.name: "firefox"
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 1
                    width: ShellTokens.pillShort; height: ShellTokens.pillHeight; radius: height / 2
                    color: ShellTokens.pillRunningOnTaskbar
                }
            }
            PanelButton { id: pressed; icon.name: "utilities-terminal"; down: true }
        }
    }

    // a quick settings flyout
    AcrylicSurface {
        id: flyout
        x: parent.width - width - 12
        y: parent.height - ShellTokens.taskbarHeight - ShellTokens.flyoutGap - height
        width: ShellTokens.quickSettingsWidth
        height: 300
        focused: true

        Grid {
            x: 24; y: 24
            columns: 3
            columnSpacing: 12
            rowSpacing: 12
            RayeToggleTile { text: "Wi-Fi"; subtitle: "Raye-5G"; iconName: "network-wireless"; split: true; checked: true }
            RayeToggleTile { text: "Bluetooth"; iconName: "network-bluetooth"; split: true }
            RayeToggleTile { text: "Airplane mode"; iconName: "airplane-mode" }
            RayeToggleTile { text: "Energy saver"; iconName: "battery-profile-powersave" }
            RayeToggleTile { text: "Night light"; iconName: "redshift-status-on"; checked: true }
            RayeToggleTile { text: "Accessibility"; iconName: "preferences-desktop-accessibility" }
        }
        Column {
            x: 24; y: 220
            spacing: 16
            RayeSlider { width: 312; from: 0; to: 100; value: 70 }
            RayeSlider { width: 312; from: 0; to: 100; value: 35 }
        }
    }

    Item { id: menuAnchor; x: 80; y: 80 }

    RayeMenu {
        id: menu
        RayeMenuItem { text: "Open"; icon.name: "document-open" }
        RayeMenuItem { text: "Pin to taskbar"; icon.name: "window-pin" }
        RayeMenuItem { text: "Show desktop icons"; checkable: true; checked: true }
        RayeMenuSeparator {}
        RayeMenuItem { text: "Task Manager"; icon.name: "utilities-system-monitor"; shortcutText: "Ctrl+Shift+Esc" }
        RayeMenuItem { text: "Close window"; icon.name: "window-close"; shortcutText: "Alt+F4" }
        RayeMenuItem { text: "Not now"; enabled: false }
    }

    Item {
        x: 420; y: 120
        width: 40; height: 40
        RayeTooltip { visible: true; text: "Search for apps, settings and documents"; delay: 0 }
    }

    Column {
        x: 420; y: 220
        spacing: 8
        Text { text: "ShellTokens"; font: ShellTokens.bodyStrong; color: ShellTokens.text }
        Text { text: "taskbar " + ShellTokens.taskbarHeight + " · button " + ShellTokens.buttonSize + " · pitch " + ShellTokens.buttonPitch; font: ShellTokens.body; color: ShellTokens.textSecondary }
        Text { text: "durations " + ShellTokens.durationFaster + " / " + ShellTokens.durationFast + " / " + ShellTokens.durationNormal + " / " + ShellTokens.durationSlow + " ms"; font: ShellTokens.body; color: ShellTokens.textSecondary }
    }
}
