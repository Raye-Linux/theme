// The Run box: 420 x 212, bottom left above the taskbar.
import QtCore
import QtQuick
import QtQuick.Controls.Basic as Basic
import QtQuick.Dialogs
import QtQuick.Effects
import QtQuick.Layouts
import QtQuick.Shapes
import QtQuick.Window
import org.kde.kirigami as Kirigami

Window {
    id: root

    required property QtObject runBox
    // Room around the box for its shadow (0 where the window cannot be
    // see-through).
    required property int shadowMargin

    width: 420 + 2 * shadowMargin
    height: 212 + 2 * shadowMargin
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: "transparent"
    title: i18nd("raye_shell_run", "Run")

    function accept() {
        root.runBox.run(field.text)
        if (root.runBox.error.length > 0) {
            field.forceActiveFocus()
            field.selectAll()
        }
    }

    Component.onCompleted: {
        if (root.runBox.history.length > 0)
            field.text = root.runBox.history[0]
        field.selectAll()
        field.forceActiveFocus()
    }

    Connections {
        target: root.runBox
        function onActivated() {
            field.forceActiveFocus()
            field.selectAll()
        }
    }

    // the Esc that closes the Browse dialog must not close the box too
    property bool browsing: false
    Timer {
        id: browseDone
        interval: 300
        onTriggered: root.browsing = false
    }

    Shortcut {
        sequences: [StandardKey.Cancel]
        enabled: !root.browsing
        onActivated: history.visible ? history.close() : root.runBox.cancel()
    }

    RectangularShadow {
        anchors.fill: card
        visible: root.shadowMargin > 0
        radius: card.radius
        blur: (Tokens.elevationDialog?.blur ?? 0) / 2
        offset.y: (Tokens.elevationDialog?.y ?? 0) / 3
        spread: 0
        color: Qt.alpha(Tokens.shadow, Tokens.elevationDialog?.opacity ?? 0)
    }

    Rectangle {
        id: card
        anchors.fill: parent
        anchors.margins: root.shadowMargin
        radius: Tokens.radiusWindow
        color: Tokens.bgBase
        border.width: 1
        border.color: Tokens.strokeSurface
        clip: true

        // Title bar: icon, "Run", close.
        Item {
            id: titleBar
            anchors { left: parent.left; right: parent.right; top: parent.top }
            height: Tokens.sizeTitlebar

            Kirigami.Icon {
                id: titleIcon
                anchors { left: parent.left; leftMargin: Tokens.s4; verticalCenter: parent.verticalCenter }
                width: Tokens.sizeIconMenu
                height: width
                source: "system-run"
            }
            Text {
                anchors { left: titleIcon.right; leftMargin: Tokens.s3; verticalCenter: parent.verticalCenter }
                text: i18nd("raye_shell_run", "Run")
                font: Tokens.windowTitle
                color: Tokens.textPrimary
            }

            MouseArea {
                id: closeButton
                anchors { right: parent.right; top: parent.top }
                width: Tokens.sizeCaptionButtonWidth
                height: Tokens.sizeCaptionButtonHeight
                hoverEnabled: true
                onClicked: root.runBox.cancel()
                Accessible.role: Accessible.Button
                Accessible.name: i18nd("raye_shell_run", "Close")

                Rectangle {
                    anchors.fill: parent
                    // only the outer corner follows the window's radius
                    topRightRadius: Tokens.radiusWindow
                    color: closeButton.pressed ? Tokens.captionClosePressed
                         : closeButton.containsMouse ? Tokens.captionClose : "transparent"
                }
                Shape {
                    anchors.centerIn: parent
                    width: 10
                    height: 10
                    preferredRendererType: Shape.CurveRenderer
                    ShapePath {
                        strokeWidth: 1
                        strokeColor: closeButton.containsMouse ? "#ffffff" : Tokens.textPrimary
                        fillColor: "transparent"
                        startX: 0; startY: 0
                        PathLine { x: 10; y: 10 }
                        PathMove { x: 10; y: 0 }
                        PathLine { x: 0; y: 10 }
                    }
                }
            }
        }

        ColumnLayout {
            anchors {
                left: parent.left; right: parent.right; top: titleBar.bottom
                leftMargin: Tokens.s7; rightMargin: Tokens.s7; topMargin: Tokens.s3
            }
            spacing: Tokens.s5

            // What the box does, or why the last line did not open.
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                spacing: Tokens.s5

                Kirigami.Icon {
                    Layout.alignment: Qt.AlignTop
                    Layout.preferredWidth: Tokens.sizeIconMultiline
                    Layout.preferredHeight: Tokens.sizeIconMultiline
                    source: root.runBox.error.length > 0 ? "dialog-error" : "system-run"
                }
                Text {
                    id: message
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    text: root.runBox.error.length > 0 ? root.runBox.error
                        : i18nd("raye_shell_run", "Type the name of a program, folder, document or Internet resource, and Raye will open it for you.")
                    font: Tokens.body
                    lineHeight: Tokens.bodyLineHeight
                    lineHeightMode: Text.FixedHeight
                    color: root.runBox.error.length > 0 ? Tokens.statusCritical : Tokens.textPrimary
                    wrapMode: Text.Wrap
                    maximumLineCount: 3
                    elide: Text.ElideRight
                    Accessible.role: Accessible.StaticText
                    Accessible.name: text
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Tokens.s4

                Text {
                    text: i18nd("raye_shell_run", "Open:")
                    font: Tokens.body
                    color: Tokens.textPrimary
                }

                Basic.TextField {
                    id: field
                    Layout.fillWidth: true
                    implicitHeight: Tokens.sizeControlHeight
                    font: Tokens.body
                    color: Tokens.textPrimary
                    selectionColor: Tokens.accent
                    selectedTextColor: Tokens.textOnAccent
                    leftPadding: Tokens.s4 - 1
                    rightPadding: historyButton.width + Tokens.s1
                    verticalAlignment: TextInput.AlignVCenter
                    selectByMouse: true
                    Accessible.name: i18nd("raye_shell_run", "Open:")
                    onTextEdited: root.runBox.clearError()
                    onAccepted: root.accept()
                    Keys.onPressed: event => {
                        if (event.key === Qt.Key_Down && (event.modifiers & Qt.AltModifier)) {
                            history.open()
                            event.accepted = true
                        }
                    }

                    background: Rectangle {
                        radius: Tokens.radiusControl
                        color: field.activeFocus ? Tokens.fillInputActive
                             : field.hovered ? Tokens.fillControlHover : Tokens.fillControl
                        border.width: 1
                        border.color: Tokens.strokeControl

                        // the bottom edge: a hairline, the accent when typing
                        Rectangle {
                            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                            anchors.leftMargin: field.activeFocus ? 0 : 1
                            anchors.rightMargin: anchors.leftMargin
                            height: field.activeFocus ? 2 : 1
                            bottomLeftRadius: Tokens.radiusControl
                            bottomRightRadius: Tokens.radiusControl
                            color: field.activeFocus ? Tokens.accent : Tokens.strokeControlBottom
                        }
                    }

                    MouseArea {
                        id: historyButton
                        anchors { right: parent.right; top: parent.top; bottom: parent.bottom; margins: 3 }
                        width: 28
                        hoverEnabled: true
                        enabled: root.runBox.history.length > 0
                        onClicked: history.visible ? history.close() : history.open()
                        Accessible.role: Accessible.Button
                        Accessible.name: i18nd("raye_shell_run", "Show recent")

                        Rectangle {
                            anchors.fill: parent
                            radius: Tokens.radiusControl
                            color: historyButton.pressed ? Tokens.fillSubtlePressed
                                 : historyButton.containsMouse ? Tokens.fillSubtleHover : "transparent"
                        }
                        Shape {
                            anchors.centerIn: parent
                            width: 10
                            height: 6
                            opacity: historyButton.enabled ? 1 : 0.4
                            preferredRendererType: Shape.CurveRenderer
                            ShapePath {
                                strokeWidth: 1
                                strokeColor: Tokens.textSecondary
                                fillColor: "transparent"
                                capStyle: ShapePath.RoundCap
                                joinStyle: ShapePath.RoundJoin
                                startX: 0.5; startY: 0.5
                                PathLine { x: 5; y: 5 }
                                PathLine { x: 9.5; y: 0.5 }
                            }
                        }
                    }

                    // The last lines, newest first.
                    Basic.Popup {
                        id: history
                        // its own window: the list goes past the bottom of the box
                        popupType: Basic.Popup.Window
                        y: field.height + Tokens.s1
                        width: field.width
                        height: Math.min(list.contentHeight, 6 * Tokens.sizeMenuItem) + 2 * Tokens.sizeMenuPadding
                        padding: Tokens.sizeMenuPadding
                        onOpened: list.currentIndex = Math.max(0, root.runBox.history.indexOf(field.text))

                        background: Rectangle {
                            radius: Tokens.radiusOverlay
                            color: Tokens.bgTertiary
                            border.width: 1
                            border.color: Tokens.strokeSurface
                        }

                        contentItem: ListView {
                            id: list
                            clip: true
                            model: root.runBox.history
                            keyNavigationEnabled: true
                            focus: true
                            Basic.ScrollBar.vertical: Basic.ScrollBar { policy: Basic.ScrollBar.AsNeeded }
                            Keys.onReturnPressed: pick(currentIndex)
                            Keys.onEnterPressed: pick(currentIndex)

                            function pick(index) {
                                field.text = root.runBox.history[index]
                                history.close()
                                field.forceActiveFocus()
                                field.selectAll()
                                root.runBox.clearError()
                            }

                            delegate: MouseArea {
                                id: row
                                required property int index
                                required property string modelData
                                width: ListView.view.width
                                height: Tokens.sizeMenuItem
                                hoverEnabled: true
                                onClicked: list.pick(index)
                                Accessible.role: Accessible.ListItem
                                Accessible.name: modelData

                                Rectangle {
                                    anchors.fill: parent
                                    radius: Tokens.radiusControl
                                    color: row.ListView.isCurrentItem || row.containsMouse ? Tokens.fillSubtleHover : "transparent"
                                }
                                Text {
                                    anchors { fill: parent; leftMargin: Tokens.s4; rightMargin: Tokens.s4 }
                                    text: row.modelData
                                    font: Tokens.body
                                    color: Tokens.textPrimary
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideMiddle
                                }
                            }
                        }
                    }
                }
            }
        }

        // OK, Cancel, Browse..., on a footer band like Windows' dialogs.
        Rectangle {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: Tokens.sizeControlHeight + 2 * Tokens.s5
            color: Tokens.bgSecondary
            bottomLeftRadius: Tokens.radiusWindow
            bottomRightRadius: Tokens.radiusWindow

            Rectangle {
                anchors { left: parent.left; right: parent.right; top: parent.top }
                height: 1
                color: Tokens.strokeDivider
            }

            RowLayout {
                anchors { right: parent.right; rightMargin: Tokens.s7; verticalCenter: parent.verticalCenter }
                spacing: Tokens.s3

                RunButton {
                    accent: true
                    text: i18nd("raye_shell_run", "OK")
                    enabled: field.text.trim().length > 0 && !root.runBox.busy
                    onClicked: root.accept()
                }
                RunButton {
                    text: i18nd("raye_shell_run", "Cancel")
                    onClicked: root.runBox.cancel()
                }
                RunButton {
                    text: i18nd("raye_shell_run", "Browse…")
                    onClicked: {
                        root.browsing = true
                        root.visible = false
                        browser.open()
                    }
                }
            }
        }
    }

    FileDialog {
        id: browser
        title: i18nd("raye_shell_run", "Browse")
        currentFolder: StandardPaths.writableLocation(StandardPaths.HomeLocation)
        nameFilters: [i18nd("raye_shell_run", "All files (*)")]
        onAccepted: {
            field.text = root.runBox.textForFile(selectedFile)
            root.runBox.clearError()
            root.visible = true
            field.forceActiveFocus()
            browseDone.restart()
        }
        onRejected: {
            root.visible = true
            field.forceActiveFocus()
            browseDone.restart()
        }
    }
}
