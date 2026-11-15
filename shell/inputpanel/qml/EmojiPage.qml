// Emoji: search, the grid of one group (or of the results), skin tone,
// and the groups along the bottom.
import QtQuick
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts

Item {
    id: page

    required property QtObject panel
    readonly property QtObject model: panel.emoji

    function takeFocus() {
        search.forceActiveFocus()
        search.selectAll()
    }

    readonly property var groups: [
        { id: 0, glyph: "🕘", label: i18nd("raye_shell_inputpanel", "Most recently used") },
        { id: 1, glyph: "😀", label: i18nd("raye_shell_inputpanel", "Smileys and emotion") },
        { id: 2, glyph: "👋", label: i18nd("raye_shell_inputpanel", "People") },
        { id: 4, glyph: "🐱", label: i18nd("raye_shell_inputpanel", "Animals and nature") },
        { id: 5, glyph: "🍔", label: i18nd("raye_shell_inputpanel", "Food and drink") },
        { id: 6, glyph: "🚗", label: i18nd("raye_shell_inputpanel", "Travel and places") },
        { id: 7, glyph: "⚽", label: i18nd("raye_shell_inputpanel", "Activities") },
        { id: 8, glyph: "💡", label: i18nd("raye_shell_inputpanel", "Objects") },
        { id: 9, glyph: "❤️", label: i18nd("raye_shell_inputpanel", "Symbols") },
        { id: 10, glyph: "🏁", label: i18nd("raye_shell_inputpanel", "Flags") },
    ]
    readonly property var tones: [
        { hand: "✋", label: i18nd("raye_shell_inputpanel", "Default") },
        { hand: "✋🏻", label: i18nd("raye_shell_inputpanel", "Light") },
        { hand: "✋🏼", label: i18nd("raye_shell_inputpanel", "Medium-light") },
        { hand: "✋🏽", label: i18nd("raye_shell_inputpanel", "Medium") },
        { hand: "✋🏾", label: i18nd("raye_shell_inputpanel", "Medium-dark") },
        { hand: "✋🏿", label: i18nd("raye_shell_inputpanel", "Dark") },
    ]

    function groupLabel(id) {
        for (const g of groups) {
            if (g.id === id)
                return g.label
        }
        return ""
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        SearchField {
            id: search
            Layout.fillWidth: true
            Layout.leftMargin: Tokens.s3
            Layout.rightMargin: Tokens.s3
            placeholderText: i18nd("raye_shell_inputpanel", "Search emoji")
            onTextChanged: page.model.query = text
            onAccepted: if (grid.count > 0) grid.pick(0)
            Keys.onDownPressed: grid.forceActiveFocus()
        }

        // The title of what the grid shows, and the skin tone.
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            Layout.leftMargin: Tokens.s4
            Layout.rightMargin: Tokens.s3

            Text {
                Layout.fillWidth: true
                text: search.text.trim().length > 0 ? i18nd("raye_shell_inputpanel", "Results")
                                                    : page.groupLabel(page.model.group)
                font: Tokens.bodyStrong
                color: Tokens.textPrimary
                elide: Text.ElideRight
            }

            PanelButton {
                id: toneButton
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                text: page.tones[page.panel.tone].hand
                font.family: "Noto Color Emoji"
                font.pixelSize: 18
                toolTip: i18nd("raye_shell_inputpanel", "Skin tone")
                onClicked: tonePicker.open()

                Basic.Popup {
                    id: tonePicker
                    x: toneButton.width - width
                    y: toneButton.height + Tokens.s1
                    padding: Tokens.sizeMenuPadding
                    background: Rectangle {
                        radius: Tokens.radiusOverlay
                        color: Tokens.bgTertiary
                        border.width: 1
                        border.color: Tokens.strokeSurface
                    }
                    contentItem: Row {
                        spacing: Tokens.s1
                        Repeater {
                            model: page.tones
                            delegate: PanelButton {
                                required property int index
                                required property var modelData
                                width: 36
                                height: 36
                                text: modelData.hand
                                font.family: "Noto Color Emoji"
                                font.pixelSize: 20
                                selected: page.panel.tone === index
                                toolTip: modelData.label
                                onClicked: {
                                    page.panel.tone = index
                                    tonePicker.close()
                                }
                            }
                        }
                    }
                }
            }
        }

        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: Tokens.s2
            Layout.rightMargin: Tokens.s2
            clip: true
            model: page.model
            cellWidth: Math.floor(width / 8)
            cellHeight: 42
            keyNavigationEnabled: true
            currentIndex: -1
            boundsBehavior: Flickable.StopAtBounds
            Basic.ScrollBar.vertical: Basic.ScrollBar { policy: Basic.ScrollBar.AsNeeded }
            Accessible.role: Accessible.List
            Accessible.name: page.groupLabel(page.model.group)

            function pick(index) {
                const item = itemAtIndex(index)
                if (item)
                    page.panel.pickEmoji(item.emoji, item.base)
            }

            onActiveFocusChanged: if (activeFocus && currentIndex < 0 && count > 0) currentIndex = 0
            Keys.onReturnPressed: pick(currentIndex)
            Keys.onEnterPressed: pick(currentIndex)
            Keys.onPressed: event => {
                // typing goes to the search box
                if (event.text.length > 0 && event.text.trim().length > 0 && !(event.modifiers & Qt.ControlModifier)) {
                    search.forceActiveFocus()
                    search.insert(search.cursorPosition, event.text)
                    event.accepted = true
                }
            }

            delegate: MouseArea {
                id: cell
                required property int index
                required property string emoji
                required property string base
                required property string name
                width: grid.cellWidth
                height: grid.cellHeight
                hoverEnabled: true
                onClicked: page.panel.pickEmoji(emoji, base)
                Accessible.role: Accessible.Button
                Accessible.name: name

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 1
                    radius: Tokens.radiusControl
                    color: cell.pressed ? Tokens.fillSubtlePressed
                         : (cell.containsMouse || (grid.activeFocus && cell.GridView.isCurrentItem)) ? Tokens.fillSubtleHover : "transparent"
                    border.width: grid.activeFocus && cell.GridView.isCurrentItem ? Tokens.sizeFocusOuter : 0
                    border.color: Tokens.focusOuter
                }
                Text {
                    anchors.centerIn: parent
                    text: cell.emoji
                    font.family: "Noto Color Emoji"
                    font.pixelSize: 24
                    scale: cell.pressed ? 0.88 : 1
                    Behavior on scale { NumberAnimation { duration: Tokens.durationFaster } }
                }
            }

            // Nothing to show
            Text {
                anchors.centerIn: parent
                width: parent.width - 2 * Tokens.s7
                visible: grid.count === 0
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                font: Tokens.body
                color: Tokens.textSecondary
                text: !page.panel.emojiLoaded ? i18nd("raye_shell_inputpanel", "Emoji couldn't be loaded.")
                    : search.text.trim().length > 0 ? i18nd("raye_shell_inputpanel", "No emoji found")
                    : i18nd("raye_shell_inputpanel", "Emoji you use show up here.")
            }
        }

        // The groups.
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            color: Tokens.bgLayer

            Rectangle {
                anchors { left: parent.left; right: parent.right; top: parent.top }
                height: 1
                color: Tokens.strokeDivider
            }

            Row {
                anchors.centerIn: parent
                spacing: 0
                Repeater {
                    model: page.groups
                    delegate: PanelButton {
                        required property var modelData
                        visible: modelData.id !== 0 || page.panel.hasRecent
                        width: 34
                        height: 36
                        text: modelData.glyph
                        font.family: "Noto Color Emoji"
                        font.pixelSize: 17
                        selected: search.text.trim().length === 0 && page.model.group === modelData.id
                        toolTip: modelData.label
                        tipAbove: true
                        onClicked: {
                            search.clear()
                            page.model.group = modelData.id
                            grid.positionViewAtBeginning()
                        }
                    }
                }
            }
        }
    }
}
