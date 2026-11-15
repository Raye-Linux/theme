// The emoji and clipboard panel: 360 x 460, near the pointer.
import QtQuick
import QtQuick.Effects
import QtQuick.Layouts
import QtQuick.Window

Window {
    id: root

    required property QtObject panel
    // Room around the panel for its shadow (0 where the window cannot be
    // see-through).
    required property int shadowMargin

    width: 360 + 2 * shadowMargin
    height: 460 + 2 * shadowMargin
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: "transparent"
    title: i18nd("raye_shell_inputpanel", "Emoji and clipboard")

    readonly property var tabs: [
        { mode: "emoji", glyph: "😊", label: i18nd("raye_shell_inputpanel", "Emoji") },
        { mode: "kaomoji", glyph: ";-)", label: i18nd("raye_shell_inputpanel", "Kaomoji") },
        { mode: "symbols", glyph: "Ω", label: i18nd("raye_shell_inputpanel", "Symbols") },
        { mode: "clipboard", glyph: "📋", label: i18nd("raye_shell_inputpanel", "Clipboard history") },
    ]

    Connections {
        target: root.panel
        function onOpened() {
            pages.currentItem.takeFocus()
        }
    }

    Shortcut {
        sequences: [StandardKey.Cancel]
        onActivated: root.panel.close()
    }

    RectangularShadow {
        anchors.fill: card
        visible: root.shadowMargin > 0
        radius: card.radius
        blur: (Tokens.elevationFlyout?.blur ?? 0) / 2
        offset.y: (Tokens.elevationFlyout?.y ?? 0) / 2
        color: Qt.alpha(Tokens.shadow, Tokens.elevationFlyout?.opacity ?? 0)
    }

    Rectangle {
        id: card
        anchors.fill: parent
        anchors.margins: root.shadowMargin
        radius: Tokens.radiusOverlay
        // see-through over the blur where the window can be, solid otherwise
        color: root.shadowMargin > 0 ? Tokens.acrylicTint : Tokens.acrylicTintSolid
        border.width: 1
        border.color: Tokens.strokeSurface
        clip: true

        // Tabs, and the close button.
        RowLayout {
            id: header
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: Tokens.s3 }
            height: 36
            spacing: Tokens.s1

            Repeater {
                model: root.tabs
                delegate: PanelButton {
                    required property var modelData
                    Layout.preferredWidth: 44
                    Layout.preferredHeight: 36
                    text: modelData.glyph
                    font.family: modelData.mode === "emoji" || modelData.mode === "clipboard" ? "Noto Color Emoji" : Tokens.body.family
                    font.pixelSize: modelData.mode === "kaomoji" ? 13 : 18
                    selected: root.panel.mode === modelData.mode
                    toolTip: modelData.label
                    onClicked: {
                        root.panel.mode = modelData.mode
                        pages.currentItem.takeFocus()
                    }
                }
            }

            Item { Layout.fillWidth: true }

            PanelButton {
                Layout.preferredWidth: 36
                Layout.preferredHeight: 36
                glyphClose: true
                toolTip: i18nd("raye_shell_inputpanel", "Close")
                onClicked: root.panel.close()
            }
        }

        StackLayout {
            id: pages
            anchors { left: parent.left; right: parent.right; top: header.bottom; bottom: parent.bottom; topMargin: Tokens.s2 }
            currentIndex: Math.max(0, ["emoji", "kaomoji", "symbols", "clipboard"].indexOf(root.panel.mode))
            readonly property Item currentItem: children[currentIndex]

            EmojiPage {
                panel: root.panel
            }
            CharPage {
                panel: root.panel
                groups: root.panel.kaomoji
                columns: 3
                textSize: 15
                titles: ({
                    happy: i18nd("raye_shell_inputpanel", "Happy"),
                    greeting: i18nd("raye_shell_inputpanel", "Greetings"),
                    love: i18nd("raye_shell_inputpanel", "Love"),
                    sad: i18nd("raye_shell_inputpanel", "Sad"),
                    angry: i18nd("raye_shell_inputpanel", "Angry"),
                    surprised: i18nd("raye_shell_inputpanel", "Surprised"),
                    shrug: i18nd("raye_shell_inputpanel", "Shrug"),
                    animals: i18nd("raye_shell_inputpanel", "Animals"),
                    classic: i18nd("raye_shell_inputpanel", "Classic")
                })
            }
            CharPage {
                panel: root.panel
                groups: root.panel.symbols
                columns: 8
                textSize: 20
                titles: ({
                    punctuation: i18nd("raye_shell_inputpanel", "Punctuation"),
                    currency: i18nd("raye_shell_inputpanel", "Currency"),
                    latin: i18nd("raye_shell_inputpanel", "Latin"),
                    geometric: i18nd("raye_shell_inputpanel", "Shapes"),
                    math: i18nd("raye_shell_inputpanel", "Math"),
                    arrows: i18nd("raye_shell_inputpanel", "Arrows"),
                    greek: i18nd("raye_shell_inputpanel", "Greek"),
                    other: i18nd("raye_shell_inputpanel", "Other")
                })
            }
            ClipboardPage {
                panel: root.panel
            }
        }
    }
}
