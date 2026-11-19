// The slim column on the left of the Windows 10 Start: the menu button at
// the top opens it over the list with the names, then at the bottom the
// account, the folders of [Start] Folders and power. 48 px wide, square.
import QtQuick
import QtQuick.Templates as T
import org.kde.kirigami as Kirigami
import org.raye.ui
import org.raye.shell

import ".."
import "../start"
import "../menulogic.js" as Menu

Item {
    id: rail

    property var backend: null
    property bool expanded: false
    readonly property var user: backend ? backend.user : null
    readonly property string fullName: user ? (user.fullName || user.loginName || "") : ""
    readonly property url face: user && user.faceIconUrl ? user.faceIconUrl : ""
    readonly property bool updatesReady: backend ? backend.updatesReady === true : false
    readonly property Item powerButton: power
    readonly property Item accountButton: account
    readonly property int railWidth: 48

    width: expanded ? 256 : railWidth
    Behavior on width { NumberAnimation { duration: ShellTokens.durationFast; easing: Motion.entrance } }
    clip: true

    // open, it lies over the list on a solid sheet
    Rectangle {
        anchors.fill: parent
        color: ShellTokens.acrylicSolid
        opacity: rail.width > rail.railWidth ? 1 : 0
        Rectangle {
            anchors.right: parent.right
            width: 1
            height: parent.height
            color: ShellTokens.separator
        }
    }

    component RailButton: T.AbstractButton {
        id: b
        property string label: ""
        // the tooltip and what a screen reader says
        property string tip: label
        property string iconName: ""
        default property alias glyph: glyphHolder.data
        width: rail.width
        height: 48
        hoverEnabled: true
        focusPolicy: Qt.TabFocus
        Accessible.name: tip
        background: Rectangle {
            color: b.down ? ShellTokens.pressFill : b.hovered ? ShellTokens.hoverFill : "transparent"
            FocusRing { radius: 0; target: b }
        }
        contentItem: Item {
            Item {
                id: glyphHolder
                width: rail.railWidth
                height: 48
                Kirigami.Icon {
                    anchors.centerIn: parent
                    width: 20
                    height: 20
                    visible: b.iconName !== ""
                    source: b.iconName
                    color: ShellTokens.text
                    isMask: true
                }
            }
            Text {
                x: rail.railWidth
                anchors.verticalCenter: parent.verticalCenter
                width: rail.width - x - 12
                text: b.label
                font: Tokens.body
                color: ShellTokens.text
                elide: Text.ElideRight
                visible: rail.width > rail.railWidth + 40
            }
        }
        RayeTooltip {
            text: b.tip
            visible: b.hovered && !rail.expanded
        }
    }

    RailButton {
        id: menuButton
        objectName: "railMenuButton"
        label: i18nd("raye_shell_start", "Start")
        tip: rail.expanded ? i18nd("raye_shell_start", "Collapse") : i18nd("raye_shell_start", "Expand")
        onClicked: rail.expanded = !rail.expanded

        // three lines
        Column {
            anchors.centerIn: parent
            spacing: 4
            Repeater {
                model: 3
                Rectangle { width: 16; height: 1.5; color: ShellTokens.text }
            }
        }
    }
    Column {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 4
        width: rail.width

        RailButton {
            id: account
            objectName: "accountButton"
            label: rail.fullName
            Accessible.role: Accessible.ButtonMenu
            onClicked: accountMenu.popup(account, 0, -accountMenu.implicitHeight)

            Item {
                anchors.centerIn: parent
                width: 24
                height: 24
                Rectangle {
                    anchors.fill: parent
                    radius: width / 2
                    color: Qt.alpha(ShellTokens.accent, 0.45)
                    visible: faceImage.status !== Image.Ready
                    Text {
                        anchors.centerIn: parent
                        text: rail.fullName.length > 0 ? rail.fullName.charAt(0).toUpperCase() : ""
                        font: Tokens.caption
                        color: ShellTokens.text
                    }
                }
                Kirigami.ShadowedImage {
                    id: faceImage
                    anchors.fill: parent
                    source: rail.face
                    radius: width / 2
                    sourceSize: Qt.size(48, 48)
                    fillMode: Image.PreserveAspectCrop
                    visible: status === Image.Ready
                }
            }
        }

        Repeater {
            id: folderButtons
            model: Menu.folderButtons(ShellSettings.startFolders)
            delegate: RailButton {
                id: folder
                required property string modelData
                objectName: "folderButton"
                label: rail.folderName(modelData)
                onClicked: rail.backend.openFolder(modelData)
                Keys.onReturnPressed: clicked()

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
        }

        RailButton {
            id: power
            objectName: "powerButton"
            label: i18nd("raye_shell_start", "Power")
            iconName: "system-shutdown"
            Accessible.role: Accessible.ButtonMenu
            onClicked: powerMenu.popup(power, 0, -powerMenu.implicitHeight)

            // updates are ready, or something asked for a restart
            Rectangle {
                objectName: "updateDot"
                visible: rail.updatesReady || ShellSettings.restartNeeded !== ""
                x: 30
                y: 12
                width: 8
                height: 8
                radius: 4
                color: ShellTokens.accent
            }
        }
    }

    AccountMenu {
        id: accountMenu
        backend: rail.backend
    }
    PowerMenu {
        id: powerMenu
        backend: rail.backend
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
    function focusItems() {
        const out = [menuButton, account]
        for (let i = 0; i < folderButtons.count; ++i)
            out.push(folderButtons.itemAt(i))
        out.push(power)
        return out
    }
}
