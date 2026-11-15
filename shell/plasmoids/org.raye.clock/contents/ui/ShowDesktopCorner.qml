// The far right of the taskbar: an 8 px strip that shows the desktop (or
// brings the windows back) on a click. Nothing is drawn until the pointer
// is on it; then a short vertical line fades in.
import QtQuick
import org.raye.shell

Item {
    id: corner

    width: ShellTokens.showDesktopWidth
    Accessible.role: Accessible.Button
    Accessible.name: i18nd("raye_shell_clock", "Show desktop")

    Rectangle {
        anchors.centerIn: parent
        width: 1
        height: 16
        color: Qt.alpha(ShellTokens.taskbarText, 0.3)
        opacity: hover.hovered ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: ShellTokens.durationFast } }
    }

    HoverHandler { id: hover }
    TapHandler {
        onTapped: Launcher.invokeShortcut("kwin", "Show Desktop")
    }
}
