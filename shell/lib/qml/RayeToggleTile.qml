// A quick settings tile: 96 x 48 with 4 px corners, its name under it.
// Off it is a quiet fill; on it is the accent. A split
// tile (Wi-Fi, Bluetooth...) toggles on its left part and opens its page
// from the 32 px "›" on the right. Turning it on fills it with the accent
// from where it was clicked; a right-click asks for its menu.
//
//   RayeToggleTile { text: i18nd(...,"Bluetooth"); iconName: "network-bluetooth"
//                    checked: bt.on; split: true; onToggled: ...; onExpandRequested: ... }
import QtQuick
import org.kde.kirigami as Kirigami
import org.raye.ui
import org.raye.shell

FocusScope {
    id: tile

    property string text: ""
    property string iconName: ""
    property bool checked: false
    // a left part that toggles and a right part that opens a page
    property bool split: false
    // shown under the name in place of nothing, e.g. the network name
    property string subtitle: ""

    signal toggled()
    signal expandRequested()
    // a right-click, at a point of the tile
    signal menuRequested(real x, real y)

    implicitWidth: ShellTokens.toggleTileWidth
    implicitHeight: ShellTokens.toggleTileHeight + 6 + label.implicitHeight
    activeFocusOnTab: true
    Accessible.role: Accessible.CheckBox
    Accessible.checkable: true
    Accessible.checked: checked
    Accessible.name: text
    Keys.onSpacePressed: tile.toggled()
    Keys.onReturnPressed: tile.toggled()
    Keys.onRightPressed: if (tile.split) tile.expandRequested()
    Keys.onMenuPressed: tile.menuRequested(width / 2, ShellTokens.toggleTileHeight / 2)

    property color ink: checked ? ShellTokens.textOnAccent : ShellTokens.text
    Behavior on ink { ColorAnimation { duration: ShellTokens.durationFast } }

    // how much of the tile the accent covers, 0 to 1, and where it grows from
    property real fillLevel: checked ? 1 : 0
    Behavior on fillLevel {
        NumberAnimation {
            duration: tile.checked ? ShellTokens.durationNormal : ShellTokens.durationFast
            easing.type: Easing.OutCubic
        }
    }
    property point fillOrigin: Qt.point(width / 2, ShellTokens.toggleTileHeight / 2)

    Item {
        id: body
        width: ShellTokens.toggleTileWidth
        height: ShellTokens.toggleTileHeight

        Rectangle {
            id: plate
            anchors.fill: parent
            radius: ShellTokens.radiusButton
            color: Tokens.fillControl
            border.width: 1
            border.color: Tokens.strokeControl

            Rectangle {
                id: wash
                objectName: "fill"
                anchors.fill: parent
                radius: ShellTokens.radiusButton
                color: ShellTokens.accent
                visible: tile.fillLevel > 0
                // a quick fade at the start hides the square corners of a small scale
                opacity: Math.min(1, tile.fillLevel * 3)
                transform: Scale {
                    origin.x: tile.fillOrigin.x
                    origin.y: tile.fillOrigin.y
                    xScale: tile.fillLevel
                    yScale: tile.fillLevel
                }
            }

            // hover and press over the whole tile, or over each part
            Rectangle {
                x: 0
                width: tile.split ? parent.width - ShellTokens.splitTileArrowWidth : parent.width
                height: parent.height
                radius: ShellTokens.radiusButton
                color: mainArea.pressed ? Qt.rgba(0, 0, 0, 0.08)
                     : mainArea.containsMouse ? Qt.rgba(1, 1, 1, tile.checked ? 0.12 : 0.05) : "transparent"
            }
            Rectangle {
                visible: tile.split
                x: parent.width - ShellTokens.splitTileArrowWidth
                width: ShellTokens.splitTileArrowWidth
                height: parent.height
                radius: ShellTokens.radiusButton
                color: arrowArea.pressed ? Qt.rgba(0, 0, 0, 0.08)
                     : arrowArea.containsMouse ? Qt.rgba(1, 1, 1, tile.checked ? 0.12 : 0.05) : "transparent"
            }
            // the line between the two parts
            Rectangle {
                visible: tile.split
                x: parent.width - ShellTokens.splitTileArrowWidth
                y: 12
                width: 1
                height: parent.height - 24
                color: tile.checked ? Qt.alpha(ShellTokens.textOnAccent, 0.3) : Tokens.strokeDivider
            }
        }

        Kirigami.Icon {
            id: glyph
            anchors.verticalCenter: parent.verticalCenter
            x: tile.split ? (parent.width - ShellTokens.splitTileArrowWidth - width) / 2 : (parent.width - width) / 2
            width: 16
            height: 16
            source: tile.iconName
            color: tile.ink
            isMask: true
            // a small pop when it changes, gone with the animations
            SequentialAnimation on scale {
                id: pop
                running: false
                NumberAnimation { to: 0.8; duration: ShellTokens.durationFaster; easing.type: Easing.OutQuad }
                NumberAnimation { to: 1; duration: ShellTokens.durationFast; easing.type: Easing.OutBack }
            }
        }
        Connections {
            target: tile
            function onCheckedChanged() {
                if (ShellTokens.durationFast > 0)
                    pop.restart()
            }
        }
        Kirigami.Icon {
            visible: tile.split
            anchors.verticalCenter: parent.verticalCenter
            x: parent.width - ShellTokens.splitTileArrowWidth + (ShellTokens.splitTileArrowWidth - width) / 2
            width: 12
            height: 12
            source: "arrow-right"
            color: tile.ink
            isMask: true
        }

        MouseArea {
            id: mainArea
            width: tile.split ? parent.width - ShellTokens.splitTileArrowWidth : parent.width
            height: parent.height
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: mouse => {
                if (mouse.button === Qt.RightButton) {
                    tile.menuRequested(mouse.x, mouse.y)
                    return
                }
                tile.fillOrigin = Qt.point(mouse.x, mouse.y)
                tile.toggled()
            }
        }
        MouseArea {
            id: arrowArea
            visible: tile.split
            x: parent.width - ShellTokens.splitTileArrowWidth
            width: ShellTokens.splitTileArrowWidth
            height: parent.height
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: mouse => {
                if (mouse.button === Qt.RightButton)
                    tile.menuRequested(x + mouse.x, mouse.y)
                else
                    tile.expandRequested()
            }
        }

        FocusRing {
            radius: ShellTokens.radiusButton
            target: tile
            shown: tile.activeFocus
        }
    }

    Text {
        id: label
        anchors.top: body.bottom
        anchors.topMargin: 6
        width: tile.width
        horizontalAlignment: Text.AlignHCenter
        text: tile.subtitle !== "" ? tile.subtitle : tile.text
        font: ShellTokens.caption
        color: ShellTokens.text
        elide: Text.ElideRight
        maximumLineCount: 1
        textFormat: Text.PlainText
    }
}
