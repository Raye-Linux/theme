// A push button the Windows way: 32 px high, 4 px corners, Body text.
// By default the accent (primary) button: accent fill, white text and a
// thin lit edge on top. With accent: false it is the standard button:
// control fill, 1 px stroke a little darker at the bottom.
//
//   AccentButton { text: qsTr("Restart now"); onClicked: ... }
import QtQuick
import QtQuick.Templates as T
import org.kde.kirigami as Kirigami
import org.raye.ui

T.Button {
    id: control

    property bool accent: true

    readonly property color fillColor: !enabled ? (accent ? Tokens.accentDisabled : Tokens.fillControlDisabled)
                                     : accent ? (down ? Tokens.accentPressed
                                                      : hovered ? Tokens.accentHover : Tokens.accent)
                                     : down ? Tokens.fillControlPressed
                                     : hovered ? Tokens.fillControlHover : Tokens.fillControl
    readonly property color textColor: !enabled ? (accent ? Qt.alpha(Tokens.textOnAccent, 0.6) : Tokens.textDisabled)
                                     : accent ? Tokens.textOnAccent
                                     : down ? Tokens.textSecondary : Tokens.textPrimary

    implicitWidth: Math.max(Tokens.sizeControlHeight * 2,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(Tokens.sizeControlHeight,
                             implicitContentHeight + topPadding + bottomPadding)
    leftPadding: 11
    rightPadding: 11
    topPadding: 5
    bottomPadding: 6
    spacing: Tokens.s3
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    font: Tokens.body
    icon.width: Tokens.sizeIconMenu
    icon.height: Tokens.sizeIconMenu

    Accessible.role: Accessible.Button
    Accessible.name: text
    Accessible.onPressAction: control.clicked()

    // Enter presses the button too, like Space
    Keys.onReturnPressed: event => { control.clicked(); event.accepted = true }
    Keys.onEnterPressed: event => { control.clicked(); event.accepted = true }

    contentItem: Row {
        spacing: control.spacing
        Kirigami.Icon {
            anchors.verticalCenter: parent.verticalCenter
            visible: control.icon.name !== "" || control.icon.source.toString() !== ""
            source: control.icon.name !== "" ? control.icon.name : control.icon.source
            implicitWidth: control.icon.width
            implicitHeight: control.icon.height
            color: control.textColor
            isMask: control.accent
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: control.text
            font: control.font
            color: control.textColor
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
    }

    background: Rectangle {
        radius: Tokens.radiusControl
        color: control.fillColor
        border.width: control.accent ? 0 : 1
        border.color: Tokens.strokeControl

        Behavior on color {
            ColorAnimation { duration: Tokens.durationFaster }
        }

        // the edge: lit on top for the accent button, darker at the
        // bottom for the standard one (only at rest and hover)
        Item {
            readonly property bool lit: control.accent
            anchors.left: parent.left
            anchors.right: parent.right
            y: lit ? 0 : parent.height - height
            height: Tokens.radiusControl
            clip: true
            visible: control.enabled && !control.down
            Rectangle {
                width: parent.width
                height: control.height
                y: parent.lit ? 0 : parent.height - height
                radius: Tokens.radiusControl
                color: "transparent"
                border.width: 1
                border.color: parent.lit ? Tokens.edgeLit : Tokens.strokeControlBottom
            }
        }

        FocusRing {
            target: control
            radius: Tokens.radiusControl
        }
    }
}
