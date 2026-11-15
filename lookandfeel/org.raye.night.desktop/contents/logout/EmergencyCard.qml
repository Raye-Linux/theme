// Ctrl+click on Power: restart right now, after one plain warning. It sits
// over the security screen and takes all the input while it is up.
import QtQuick
import QtQuick.Templates as T
import org.raye.signin

FocusScope {
    id: card

    signal confirmed()
    signal closed()

    function open() {
        visible = true
        forceActiveFocus()
    }

    function close() {
        visible = false
        closed()
    }

    visible: false
    Keys.onEscapePressed: card.close()
    Keys.onReturnPressed: card.confirmed()
    Keys.onEnterPressed: card.confirmed()

    // the dim also swallows clicks and the wheel
    Rectangle {
        anchors.fill: parent
        color: RayePalette.highContrast ? "#000000" : Qt.rgba(0.05, 0.04, 0.09, 0.72)
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            onWheel: wheel => wheel.accepted = true
        }
    }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - 2 * RayePalette.edge, Math.round(456 * RayePalette.textScale))
        height: body.implicitHeight + 48
        radius: 12
        color: RayePalette.flyoutFill
        border.width: 1
        border.color: RayePalette.flyoutBorder

        Column {
            id: body
            anchors.fill: parent
            anchors.margins: 24
            spacing: 12

            Text {
                width: parent.width
                text: Strings.tr.emergencyRestart
                color: RayePalette.text
                wrapMode: Text.WordWrap
                font.family: RayePalette.displayFamily
                font.weight: Font.DemiBold
                font.pixelSize: Math.round(20 * RayePalette.textScale)
            }

            Text {
                width: parent.width
                text: Strings.tr.emergencyWarning
                color: RayePalette.textSecondary
                wrapMode: Text.WordWrap
                font.family: RayePalette.family
                font.pixelSize: Math.round(14 * RayePalette.textScale)
            }

            Row {
                anchors.right: parent.right
                spacing: 8
                topPadding: 8

                Repeater {
                    model: [
                        { id: "emergencyOk", text: Strings.tr.ok, accent: true },
                        { id: "emergencyCancel", text: Strings.tr.cancel, accent: false }
                    ]
                    delegate: T.AbstractButton {
                        id: button
                        required property var modelData
                        objectName: modelData.id
                        width: Math.max(96, label.implicitWidth + 32)
                        height: 36
                        hoverEnabled: true
                        focusPolicy: Qt.NoFocus
                        Accessible.role: Accessible.Button
                        Accessible.name: modelData.text
                        onClicked: {
                            if (modelData.accent)
                                card.confirmed()
                            else
                                card.close()
                        }
                        background: Rectangle {
                            radius: RayePalette.controlRadius
                            color: button.modelData.accent
                                   ? (button.pressed ? Qt.darker(RayePalette.accent, 1.2) : RayePalette.accent)
                                   : (button.hovered ? RayePalette.hoverFill : "transparent")
                            border.width: button.modelData.accent ? 0 : 1
                            border.color: RayePalette.ring
                        }
                        contentItem: Text {
                            id: label
                            text: button.modelData.text
                            color: RayePalette.text
                            font.family: RayePalette.family
                            font.weight: Font.DemiBold
                            font.pixelSize: Math.round(14 * RayePalette.textScale)
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
            }
        }
    }
}
