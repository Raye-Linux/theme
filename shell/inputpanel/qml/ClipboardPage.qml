// Clipboard history: pinned items first, then what was copied, newest
// first. Click pastes; pin and delete on each card; Clear all keeps pins.
import QtQuick
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts
import QtQuick.Shapes

Item {
    id: page

    required property QtObject panel

    function takeFocus() {
        list.forceActiveFocus()
        if (list.currentIndex < 0 && list.count > 0)
            list.currentIndex = 0
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Tokens.s2

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Tokens.s4
            Layout.rightMargin: Tokens.s3
            Layout.preferredHeight: 32

            Text {
                Layout.fillWidth: true
                text: i18nd("raye_shell_inputpanel", "Clipboard history")
                font: Tokens.bodyStrong
                color: Tokens.textPrimary
                elide: Text.ElideRight
            }
            PanelButton {
                Layout.preferredHeight: 32
                Layout.preferredWidth: implicitContentWidth + 2 * Tokens.s4
                visible: page.panel.clipboardAvailable && list.count > 0
                text: i18nd("raye_shell_inputpanel", "Clear all")
                onClicked: page.panel.clearAll()
                contentItem: Text {
                    text: parent.text
                    font: Tokens.body
                    color: Tokens.accentText
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: Tokens.s3
            Layout.rightMargin: Tokens.s3
            Layout.bottomMargin: Tokens.s3
            clip: true
            spacing: Tokens.s2
            model: page.panel.clipboard
            keyNavigationEnabled: true
            boundsBehavior: Flickable.StopAtBounds
            Basic.ScrollBar.vertical: Basic.ScrollBar { policy: Basic.ScrollBar.AsNeeded }
            Accessible.role: Accessible.List
            Accessible.name: i18nd("raye_shell_inputpanel", "Clipboard history")
            Keys.onReturnPressed: if (currentItem) page.panel.pickClipboard(currentItem.modelData.text)
            Keys.onEnterPressed: if (currentItem) page.panel.pickClipboard(currentItem.modelData.text)
            Keys.onDeletePressed: if (currentItem) page.panel.remove(currentItem.modelData.text)

            delegate: MouseArea {
                id: card
                required property int index
                required property var modelData
                readonly property bool active: containsMouse || (list.activeFocus && ListView.isCurrentItem)
                width: ListView.view.width - (list.contentHeight > list.height ? 8 : 0)
                height: Math.max(56, preview.implicitHeight + 2 * Tokens.s4)
                hoverEnabled: true
                onClicked: page.panel.pickClipboard(modelData.text)
                Accessible.role: Accessible.Button
                Accessible.name: modelData.text.substring(0, 200)

                Rectangle {
                    anchors.fill: parent
                    radius: Tokens.radiusControl
                    color: card.pressed ? Tokens.fillControlPressed : card.active ? Tokens.fillControlHover : Tokens.fillControl
                    border.width: list.activeFocus && card.ListView.isCurrentItem ? Tokens.sizeFocusOuter : 1
                    border.color: list.activeFocus && card.ListView.isCurrentItem ? Tokens.focusOuter : Tokens.strokeControl
                }

                Text {
                    id: preview
                    anchors { left: parent.left; right: actions.left; top: parent.top; margins: Tokens.s4; rightMargin: Tokens.s2 }
                    // the start of the text only: a long item stays one card
                    text: card.modelData.text.substring(0, 400)
                    textFormat: Text.PlainText
                    font: Tokens.body
                    color: Tokens.textPrimary
                    wrapMode: Text.WrapAnywhere
                    maximumLineCount: 3
                    elide: Text.ElideRight
                }

                Column {
                    id: actions
                    anchors { right: parent.right; top: parent.top; margins: Tokens.s1 }
                    spacing: 0

                    PanelButton {
                        width: 32
                        height: 26
                        visible: card.active || card.modelData.pinned
                        selected: card.modelData.pinned
                        toolTip: card.modelData.pinned ? i18nd("raye_shell_inputpanel", "Unpin") : i18nd("raye_shell_inputpanel", "Pin")
                        onClicked: card.modelData.pinned ? page.panel.unpin(card.modelData.text) : page.panel.pin(card.modelData.text)
                        contentItem: Item {
                            // a push pin
                            Shape {
                                anchors.centerIn: parent
                                width: 12
                                height: 14
                                preferredRendererType: Shape.CurveRenderer
                                ShapePath {
                                    strokeWidth: 1
                                    strokeColor: card.modelData.pinned ? Tokens.accentText : Tokens.textSecondary
                                    fillColor: card.modelData.pinned ? Tokens.accentText : "transparent"
                                    joinStyle: ShapePath.RoundJoin
                                    startX: 3; startY: 0.5
                                    PathLine { x: 9; y: 0.5 }
                                    PathLine { x: 8; y: 5 }
                                    PathLine { x: 11; y: 8 }
                                    PathLine { x: 1; y: 8 }
                                    PathLine { x: 4; y: 5 }
                                    PathLine { x: 3; y: 0.5 }
                                }
                                ShapePath {
                                    strokeWidth: 1
                                    strokeColor: card.modelData.pinned ? Tokens.accentText : Tokens.textSecondary
                                    capStyle: ShapePath.RoundCap
                                    startX: 6; startY: 8
                                    PathLine { x: 6; y: 13.5 }
                                }
                            }
                        }
                    }
                    PanelButton {
                        width: 32
                        height: 26
                        visible: card.active
                        glyphClose: true
                        glyphColor: Tokens.textSecondary
                        toolTip: i18nd("raye_shell_inputpanel", "Delete")
                        onClicked: page.panel.remove(card.modelData.text)
                    }
                }
            }

            // Nothing in the history
            ColumnLayout {
                anchors.centerIn: parent
                width: parent.width - 2 * Tokens.s7
                visible: list.count === 0
                spacing: Tokens.s2

                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: page.panel.clipboardAvailable ? i18nd("raye_shell_inputpanel", "Nothing here yet")
                                                        : i18nd("raye_shell_inputpanel", "Clipboard history isn't available right now.")
                    font: Tokens.bodyStrong
                    color: Tokens.textPrimary
                    wrapMode: Text.Wrap
                }
                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    visible: page.panel.clipboardAvailable
                    text: i18nd("raye_shell_inputpanel", "Copy something and it will show up here.")
                    font: Tokens.body
                    color: Tokens.textSecondary
                    wrapMode: Text.Wrap
                }
            }
        }
    }
}
