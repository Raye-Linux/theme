// The Wi-Fi line of the network page while the cable is in use: folded, it
// only says whether Wi-Fi is on; a click opens the networks under it, and
// then its switch shows.
import QtQuick
import org.kde.kirigami as Kirigami
import org.raye.ui
import org.raye.shell

Rectangle {
    id: fold

    property bool expanded: false
    property bool wifiOn: false

    signal clicked()
    signal switchToggled(bool on)

    width: parent ? parent.width : 0
    height: 48
    radius: ShellTokens.radiusButton
    color: area.containsMouse ? ShellTokens.hoverFill : "transparent"
    Accessible.role: Accessible.Button
    Accessible.name: i18nd("raye_shell_quicksettings", "Wi-Fi")
    Accessible.description: status.text
    Accessible.onPressAction: fold.clicked()

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        onClicked: fold.clicked()
    }
    Kirigami.Icon {
        x: 12
        y: 16
        width: 16
        height: 16
        source: fold.wifiOn ? "network-wireless-signal-excellent-symbolic" : "network-wireless-disconnected-symbolic"
        color: ShellTokens.text
        isMask: true
    }
    Column {
        x: 40
        y: 7
        width: parent.width - x - (wifiSwitch.visible ? wifiSwitch.width + 52 : 40)
        Text {
            width: parent.width
            text: i18nd("raye_shell_quicksettings", "Wi-Fi")
            font: ShellTokens.body
            color: ShellTokens.text
            elide: Text.ElideRight
        }
        Text {
            id: status
            width: parent.width
            text: fold.wifiOn ? i18nd("raye_shell_quicksettings", "On") : i18nd("raye_shell_quicksettings", "Off")
            font: ShellTokens.caption
            color: ShellTokens.textSecondary
            elide: Text.ElideRight
        }
    }
    Switch {
        id: wifiSwitch
        objectName: "wifiSwitch"
        visible: fold.expanded
        anchors.right: chevron.left
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        showStateText: false
        text: i18nd("raye_shell_quicksettings", "Wi-Fi")
        checked: fold.wifiOn
        onToggled: fold.switchToggled(checked)
    }
    Kirigami.Icon {
        id: chevron
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        width: 16
        height: 16
        source: fold.expanded ? "go-up-symbolic" : "go-down-symbolic"
        color: ShellTokens.textSecondary
        isMask: true
    }
}
