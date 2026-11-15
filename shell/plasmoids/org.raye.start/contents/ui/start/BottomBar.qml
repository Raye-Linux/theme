// Start's bottom bar: 64 px, a little darker than the rest, a line on
// top. On the left the account (picture and name; its menu: account
// settings in Raye Settings, Lock, Sign out, Switch user), on the right
// the power button (Sleep, Shut down, Restart; "Update and ..." when Raye
// Update has updates ready, and a dot on the button then).
import QtQuick
import QtQuick.Templates as T
import org.kde.kirigami as Kirigami
import org.raye.ui
import org.raye.shell

import "../menulogic.js" as Menu

Item {
    id: bar

    property var backend: null
    readonly property var user: backend ? backend.user : null
    readonly property var sessions: backend ? backend.sessions : null
    readonly property string fullName: user ? (user.fullName || user.loginName || "") : ""
    readonly property url face: user && user.faceIconUrl ? user.faceIconUrl : ""
    readonly property bool updatesReady: backend ? backend.updatesReady === true : false
    readonly property Item accountButton: account
    readonly property Item powerButton: power

    implicitHeight: 64

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, Tokens.night ? 0.15 : 0.04)
        // only the bottom corners follow the popup's rounding
        bottomLeftRadius: ShellTokens.radiusFlyout
        bottomRightRadius: ShellTokens.radiusFlyout
    }
    Rectangle {
        width: parent.width
        height: 1
        color: Tokens.strokeDivider
    }

    T.AbstractButton {
        id: account
        objectName: "accountButton"
        anchors { left: parent.left; leftMargin: 52; verticalCenter: parent.verticalCenter }
        height: 40
        width: Math.min(implicitContentWidth + 24, folderRow.x - x - 8)
        hoverEnabled: true
        focusPolicy: Qt.TabFocus
        Accessible.name: bar.fullName
        Accessible.role: Accessible.ButtonMenu
        onClicked: accountMenu.popup(account, 0, -accountMenu.implicitHeight - 4)

        background: Rectangle {
            radius: Tokens.radiusControl
            color: account.down ? ShellTokens.pressFill : account.hovered ? ShellTokens.hoverFill : "transparent"
            FocusRing { radius: Tokens.radiusControl; target: account }
        }
        contentItem: Row {
            spacing: 12
            leftPadding: 12
            Item {
                width: 32
                height: 32
                anchors.verticalCenter: parent.verticalCenter
                Rectangle {
                    anchors.fill: parent
                    radius: width / 2
                    color: Qt.alpha(ShellTokens.accent, 0.35)
                    visible: faceImage.status !== Image.Ready
                    Text {
                        anchors.centerIn: parent
                        text: bar.fullName.length > 0 ? bar.fullName.charAt(0).toUpperCase() : ""
                        font: Tokens.bodyStrong
                        color: ShellTokens.text
                    }
                }
                Kirigami.ShadowedImage {
                    id: faceImage
                    anchors.fill: parent
                    source: bar.face
                    radius: width / 2
                    sourceSize: Qt.size(64, 64)
                    fillMode: Image.PreserveAspectCrop
                    visible: status === Image.Ready
                }
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: bar.fullName
                font: Tokens.body
                color: ShellTokens.text
                elide: Text.ElideRight
                width: Math.min(implicitWidth, Math.max(40, account.width - 68))
            }
        }
    }

    RayeMenu {
        id: accountMenu
        RayeMenuItem {
            text: i18nd("raye_shell_start", "Change account settings")
            icon.name: "user-identity"
            onTriggered: bar.backend.power("account")
        }
        RayeMenuSeparator {}
        RayeMenuItem {
            text: i18nd("raye_shell_start", "Lock")
            icon.name: "system-lock-screen"
            enabled: !bar.sessions || bar.sessions.canLock !== false
            onTriggered: bar.backend.power("lock")
        }
        RayeMenuItem {
            text: i18nd("raye_shell_start", "Sign out")
            icon.name: "system-log-out"
            enabled: !bar.sessions || bar.sessions.canLogout !== false
            onTriggered: bar.backend.power("signout")
        }
        RayeMenuItem {
            objectName: "switchUser"
            text: i18nd("raye_shell_start", "Switch user")
            icon.name: "system-switch-user"
            visible: bar.sessions ? bar.sessions.canSwitchUser === true : false
            height: visible ? implicitHeight : 0
            onTriggered: bar.backend.power("switch")
        }
    }

    T.AbstractButton {
        id: power
        objectName: "powerButton"
        anchors { right: parent.right; rightMargin: 52; verticalCenter: parent.verticalCenter }
        width: 40
        height: 40
        hoverEnabled: true
        focusPolicy: Qt.TabFocus
        Accessible.name: i18nd("raye_shell_start", "Power")
        Accessible.role: Accessible.ButtonMenu
        onClicked: powerMenu.popup(power, power.width - powerMenu.implicitWidth, -powerMenu.implicitHeight - 4)

        background: Rectangle {
            radius: Tokens.radiusControl
            color: power.down ? ShellTokens.pressFill : power.hovered ? ShellTokens.hoverFill : "transparent"
            FocusRing { radius: Tokens.radiusControl; target: power }
        }
        contentItem: Item {
            Kirigami.Icon {
                anchors.centerIn: parent
                width: 20
                height: 20
                source: "system-shutdown"
                color: ShellTokens.text
                isMask: true
            }
            // updates are ready, or something asked for a restart: a dot in the accent
            Rectangle {
                objectName: "updateDot"
                visible: bar.updatesReady || ShellSettings.restartNeeded !== ""
                x: parent.width - 12
                y: 6
                width: 8
                height: 8
                radius: 4
                color: ShellTokens.accent
            }
        }
        RayeTooltip {
            text: i18nd("raye_shell_start", "Power")
            visible: power.hovered && !powerMenu.visible
        }
    }

    RayeMenu {
        id: powerMenu
        objectName: "powerMenu"
        RayeMenuItem {
            text: i18nd("raye_shell_start", "Sleep")
            icon.name: "system-suspend"
            visible: !bar.sessions || bar.sessions.canSuspend !== false
            height: visible ? implicitHeight : 0
            onTriggered: bar.backend.power("sleep")
        }
        RayeMenuItem {
            text: i18nd("raye_shell_start", "Update and shut down")
            icon.name: "system-shutdown"
            visible: bar.updatesReady
            height: visible ? implicitHeight : 0
            onTriggered: bar.backend.power("update-shutdown")
        }
        RayeMenuItem {
            text: i18nd("raye_shell_start", "Shut down")
            icon.name: "system-shutdown"
            enabled: !bar.sessions || bar.sessions.canShutdown !== false
            onTriggered: bar.backend.power("shutdown")
        }
        RayeMenuItem {
            text: i18nd("raye_shell_start", "Update and restart")
            icon.name: "system-reboot"
            visible: bar.updatesReady
            height: visible ? implicitHeight : 0
            onTriggered: bar.backend.power("update-restart")
        }
        RayeMenuItem {
            text: i18nd("raye_shell_start", "Restart")
            icon.name: "system-reboot"
            enabled: !bar.sessions || bar.sessions.canReboot !== false
            onTriggered: bar.backend.power("restart")
        }
        RayeMenuItem {
            objectName: "restartToWindows"
            readonly property bool wanted: bar.backend ? bar.backend.canRestartToWindows === true : false
            text: i18nd("raye_shell_start", "Restart to Windows")
            icon.name: "system-reboot"
            visible: wanted
            height: visible ? implicitHeight : 0
            onTriggered: bar.backend.restartToWindows()
        }
    }

    // [Start] Folders: a button each, before power
    Row {
        id: folderRow
        objectName: "folderRow"
        anchors { right: power.left; rightMargin: 4; verticalCenter: parent.verticalCenter }
        spacing: 4
        Repeater {
            id: folderButtons
            model: Menu.folderButtons(ShellSettings.startFolders)
            delegate: T.AbstractButton {
                id: folder
                required property string modelData
                objectName: "folderButton"
                width: 40
                height: 40
                hoverEnabled: true
                focusPolicy: Qt.TabFocus
                Accessible.name: bar.folderName(modelData)
                onClicked: bar.backend.openFolder(modelData)
                Keys.onReturnPressed: clicked()
                background: Rectangle {
                    radius: Tokens.radiusControl
                    color: folder.down ? ShellTokens.pressFill : folder.hovered ? ShellTokens.hoverFill : "transparent"
                    FocusRing { radius: Tokens.radiusControl; target: folder }
                }
                contentItem: Item {
                    Kirigami.Icon {
                        anchors.centerIn: parent
                        width: 20
                        height: 20
                        visible: Menu.folderGlyph(folder.modelData) === ""
                        source: Menu.folderIcon(folder.modelData)
                        color: ShellTokens.text
                        isMask: true
                    }
                    FolderGlyph {
                        anchors.centerIn: parent
                        width: 20
                        height: 20
                        visible: kind !== ""
                        kind: Menu.folderGlyph(folder.modelData)
                        color: ShellTokens.text
                    }
                }
                RayeTooltip {
                    text: bar.folderName(folder.modelData)
                    visible: folder.hovered
                }
            }
        }
    }

    function folderName(key) {
        switch (key) {
        case "settings": return i18nd("raye_shell_start", "Settings")
        case "files": return i18nd("raye_shell_start", "Files")
        case "documents": return i18nd("raye_shell_start", "Documents")
        case "downloads": return i18nd("raye_shell_start", "Downloads")
        case "music": return i18nd("raye_shell_start", "Music")
        case "pictures": return i18nd("raye_shell_start", "Pictures")
        case "videos": return i18nd("raye_shell_start", "Videos")
        case "network": return i18nd("raye_shell_start", "Network")
        case "personal": return i18nd("raye_shell_start", "Personal folder")
        }
        return ""
    }
    // for Tab, in order
    function folderItems() {
        const out = []
        for (let i = 0; i < folderButtons.count; ++i)
            out.push(folderButtons.itemAt(i))
        return out
    }

    function openPowerMenu() { power.clicked() }
    function openAccountMenu() { account.clicked() }
}
