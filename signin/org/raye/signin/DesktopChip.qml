// Under the password: the desktop this sign-in opens, a tiny picture of
// it and its name ("KDE Plasma", "Hyprland"). Shown only when there is a
// choice. When another desktop is picked the old one slides up and fades
// as the new one comes in, so the choice is seen where the eyes are.
// Clicking it (or Enter) opens the Desktop flyout of the corner bar.
//
// In:  desktop  "plasma", "hyprland" or "" (hidden)
// Out: clicked()
import QtQuick
import QtQuick.Templates as T

T.AbstractButton {
    id: chip

    property string desktop: ""

    function nameOf(key) {
        return key === "hyprland" ? Strings.tr.sessionHyprland : Strings.tr.sessionPlasma
    }

    visible: desktop !== ""
    implicitWidth: Math.max(incoming.implicitWidth, outgoing.implicitWidth) + 16
    implicitHeight: Math.max(28, incoming.implicitHeight + 8)
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    clip: true

    Accessible.role: Accessible.Button
    Accessible.name: Strings.tr.desktopNamed.arg(nameOf(desktop))

    Keys.onReturnPressed: clicked()
    Keys.onEnterPressed: clicked()

    onDesktopChanged: {
        if (!swap.running && incoming.key !== "" && incoming.key !== desktop) {
            outgoing.key = incoming.key
            swap.restart()
        }
        incoming.key = desktop
    }
    Component.onCompleted: incoming.key = desktop

    component Line: Row {
        property string key: ""
        spacing: 8
        // a desktop 24 x 15 px: windows over a taskbar, or tiles under a bar
        Item {
            anchors.verticalCenter: parent.verticalCenter
            width: 24; height: 15
            Rectangle {
                anchors.fill: parent
                radius: 2
                color: "transparent"
                border.width: 1
                border.color: RayePalette.textSecondary
            }
            Item {
                anchors.fill: parent
                anchors.margins: 2
                visible: parent.parent.key !== "hyprland"
                Rectangle { x: 1; y: 1; width: 11; height: 6; radius: 1; color: RayePalette.textSecondary; opacity: 0.6 }
                Rectangle { x: 7; y: 3; width: 12; height: 6; radius: 1; color: RayePalette.accent }
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 2; color: RayePalette.textSecondary; opacity: 0.6 }
            }
            Item {
                anchors.fill: parent
                anchors.margins: 2
                visible: parent.parent.key === "hyprland"
                Rectangle { width: parent.width; height: 2; radius: 1; color: RayePalette.textSecondary; opacity: 0.6 }
                Rectangle { y: 3; width: 11; height: parent.height - 3; radius: 1; color: RayePalette.accent }
                Rectangle { x: 12; y: 3; width: parent.width - 12; height: (parent.height - 4) / 2; radius: 1; color: RayePalette.textSecondary; opacity: 0.6 }
                Rectangle { x: 12; y: 4 + (parent.height - 4) / 2; width: parent.width - 12; height: (parent.height - 4) / 2; radius: 1; color: RayePalette.textSecondary; opacity: 0.6 }
            }
        }
        ShadowText {
            anchors.verticalCenter: parent.verticalCenter
            text: chip.nameOf(parent.key)
            font.pixelSize: Math.round(14 * RayePalette.textScale)
            color: RayePalette.textSecondary
        }
    }

    contentItem: Item {
        Line {
            id: outgoing
            objectName: "outgoing"
            anchors.horizontalCenter: parent.horizontalCenter
            y: (parent.height - height) / 2
            opacity: 0
        }
        Line {
            id: incoming
            objectName: "incoming"
            anchors.horizontalCenter: parent.horizontalCenter
            y: (parent.height - height) / 2
        }
    }

    background: Rectangle {
        radius: RayePalette.controlRadius
        color: chip.pressed ? RayePalette.pressFill : chip.hovered ? RayePalette.hoverFill : "transparent"
        border.width: chip.visualFocus ? RayePalette.focusWidth : 0
        border.color: RayePalette.focus
    }

    ParallelAnimation {
        id: swap
        NumberAnimation { target: outgoing; property: "opacity"; from: 1; to: 0; duration: Motion.slow; easing.type: Motion.leave }
        NumberAnimation {
            target: outgoing; property: "y"
            from: (chip.availableHeight - outgoing.height) / 2; to: (chip.availableHeight - outgoing.height) / 2 - 12
            duration: Motion.slow; easing.type: Motion.leave
        }
        NumberAnimation { target: incoming; property: "opacity"; from: 0; to: 1; duration: Motion.slow; easing.type: Motion.enter }
        NumberAnimation {
            target: incoming; property: "y"
            from: (chip.availableHeight - incoming.height) / 2 + 12; to: (chip.availableHeight - incoming.height) / 2
            duration: Motion.slow; easing.type: Motion.enter
        }
    }
}
