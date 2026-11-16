// A row of a quick settings page: a glyph, a name and a line under it.
// A click selects it and shows its button under it (Connect, Disconnect),
// with a password field first when the network asks for one. A row with
// no button (a sound output) does its job on the click itself.
import QtQuick
import org.kde.kirigami as Kirigami
import org.raye.ui
import org.raye.shell

Rectangle {
    id: row

    property string iconName: ""
    property string name: ""
    property string status: ""
    // drawn in the accent: what is in use
    property bool current: false
    property bool selected: false
    // the button shown when selected; empty for a row that acts on a click
    property string actionText: ""
    property bool askPassword: false
    readonly property string password: passwordInput.text
    // what was typed before the list was rebuilt (a Wi-Fi scan rebuilds it)
    property string savedPassword: ""
    property bool revealPassword: false
    signal passwordEdited(string text)

    signal clicked()
    signal action()

    width: parent ? parent.width : 0
    height: 48 + (selected && actionText !== "" ? extra.height : 0)
    radius: ShellTokens.radiusButton
    color: selected ? Tokens.fillControl : area.containsMouse ? ShellTokens.hoverFill : "transparent"
    clip: true
    Accessible.role: Accessible.ListItem
    Accessible.name: row.name

    onSelectedChanged: if (!selected) passwordInput.text = ""

    // the line on the left of what is in use, like the Windows lists
    Rectangle {
        visible: row.current
        x: 0
        y: 16
        width: 3
        height: 16
        radius: 1.5
        color: ShellTokens.accent
    }
    Kirigami.Icon {
        x: 12
        y: 16
        width: 16
        height: 16
        source: row.iconName
        color: row.current ? ShellTokens.accent : ShellTokens.text
        isMask: true
    }
    Column {
        x: 40
        y: row.status !== "" ? 7 : 15
        width: parent.width - 52
        Text {
            width: parent.width
            text: row.name
            font: ShellTokens.body
            color: ShellTokens.text
            elide: Text.ElideRight
            textFormat: Text.PlainText
        }
        Text {
            visible: row.status !== ""
            width: parent.width
            text: row.status
            font: ShellTokens.caption
            color: ShellTokens.textSecondary
            elide: Text.ElideRight
        }
    }
    MouseArea {
        id: area
        width: parent.width
        height: 48
        hoverEnabled: true
        onClicked: row.clicked()
    }

    Column {
        id: extra
        y: 48
        x: 40
        width: parent.width - 52
        spacing: 8
        bottomPadding: 12
        visible: row.selected && row.actionText !== ""

        Rectangle {
            visible: row.askPassword
            width: parent.width
            height: 32
            radius: ShellTokens.radiusButton
            color: Tokens.fillControl
            border.width: 1
            border.color: passwordInput.activeFocus ? ShellTokens.accent : Tokens.strokeControl
            TextInput {
                id: passwordInput
                objectName: "passwordField"
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 36
                verticalAlignment: TextInput.AlignVCenter
                echoMode: row.revealPassword ? TextInput.Normal : TextInput.Password
                font: ShellTokens.body
                color: ShellTokens.text
                selectByMouse: true
                clip: true
                onAccepted: row.action()
                onTextEdited: row.passwordEdited(text)
                Component.onCompleted: text = row.savedPassword
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: passwordInput.text === ""
                    text: i18nd("raye_shell_quicksettings", "Enter the network security key")
                    font: ShellTokens.body
                    color: ShellTokens.textSecondary
                    width: parent.width
                    elide: Text.ElideRight
                }
            }
            // show what was typed, to check it before connecting
            MouseArea {
                objectName: "revealPassword"
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: 32
                height: parent.height
                visible: passwordInput.text !== ""
                cursorShape: Qt.PointingHandCursor
                Accessible.role: Accessible.Button
                Accessible.name: row.revealPassword ? i18nd("raye_shell_quicksettings", "Hide the key")
                                                    : i18nd("raye_shell_quicksettings", "Show the key")
                onClicked: row.revealPassword = !row.revealPassword
                Kirigami.Icon {
                    anchors.centerIn: parent
                    width: 16
                    height: 16
                    source: row.revealPassword ? "view-hidden" : "view-visible"
                    color: ShellTokens.textSecondary
                }
            }
        }
        AccentButton {
            objectName: "rowAction"
            anchors.right: parent.right
            accent: !row.current
            text: row.actionText
            enabled: !row.askPassword || passwordInput.text.length > 0
            onClicked: row.action()
        }
    }
}
