// A page of quick settings that opens in place of the tiles, the way
// Windows 11 does it: Back and a title on top, a short list, and at the
// bottom the one link that leaves for Raye Settings.
import QtQuick
import org.kde.kirigami as Kirigami
import org.raye.ui
import org.raye.shell

Column {
    id: page

    property string title: ""
    // the label of the link at the bottom; empty for none
    property string linkText: ""
    // a switch next to the title (Wi-Fi, Bluetooth), or none
    property bool hasSwitch: false
    property bool switchOn: false
    // the list, with its room capped so the panel never runs off the screen
    default property alias rows: list.data
    property int maxListHeight: 288
    // the height with the list full, so the window can be made that tall once
    readonly property int tallest: topPadding + 40 + maxListHeight
                                   + (linkText !== "" ? 49 : 0) + bottomPadding

    signal back()
    signal linkClicked()
    signal switchToggled(bool on)

    topPadding: 8
    bottomPadding: 8

    Item {
        width: parent.width
        height: 40
        PanelButton {
            objectName: "backButton"
            x: 8
            anchors.verticalCenter: parent.verticalCenter
            width: 32
            height: 32
            onTaskbar: false
            toolTipText: i18nd("raye_shell_quicksettings", "Back")
            onClicked: page.back()
            Kirigami.Icon {
                anchors.centerIn: parent
                width: 16
                height: 16
                source: "go-previous-symbolic"
                color: ShellTokens.text
                isMask: true
            }
        }
        Text {
            x: 44
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - x - (pageSwitch.visible ? pageSwitch.width + 24 : 16)
            text: page.title
            font: ShellTokens.bodyStrong
            color: ShellTokens.text
            elide: Text.ElideRight
        }
        Switch {
            id: pageSwitch
            objectName: "pageSwitch"
            visible: page.hasSwitch
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            showStateText: false
            text: page.title
            checked: page.switchOn
            onToggled: page.switchToggled(checked)
        }
    }

    Flickable {
        x: 8
        width: parent.width - 16
        height: Math.min(list.implicitHeight, page.maxListHeight)
        contentHeight: list.implicitHeight
        clip: contentHeight > height
        interactive: contentHeight > height
        boundsBehavior: Flickable.StopAtBounds
        Column {
            id: list
            width: parent.width
            spacing: 2
        }
    }

    Item {
        width: parent.width
        height: 8
        visible: page.linkText !== ""
    }
    Rectangle {
        visible: page.linkText !== ""
        x: 8
        width: parent.width - 16
        height: 1
        color: ShellTokens.separator
    }
    Item {
        visible: page.linkText !== ""
        width: parent.width
        height: 40
        Text {
            id: link
            objectName: "settingsLink"
            x: 16
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(implicitWidth, parent.width - 32)
            text: page.linkText
            font: ShellTokens.body
            color: linkArea.containsMouse ? Qt.lighter(Tokens.accentText, 1.15) : Tokens.accentText
            elide: Text.ElideRight
            signal clicked()
            onClicked: page.linkClicked()
            MouseArea {
                id: linkArea
                anchors.fill: parent
                anchors.margins: -6
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: link.clicked()
                Accessible.role: Accessible.Link
                Accessible.name: link.text
            }
        }
    }
}
