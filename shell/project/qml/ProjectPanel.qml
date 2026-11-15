// What the Project panel shows: four rows, the one in use marked, and a
// link to Display settings. Up and Down move, Enter or a click picks, Esc
// leaves. `controller` is the part that knows the screens.
import QtQuick
import org.kde.kirigami as Kirigami
import org.raye.ui
import org.raye.shell

FocusScope {
    id: panel

    property QtObject controller
    property bool shown: true
    // where the keys are; starts on the choice in use
    property int focusRow: 0

    readonly property int margin: 16
    readonly property var modes: ["pc-only", "duplicate", "extend", "second-only"]
    readonly property bool canChoose: !!controller && controller.canChoose

    signal requestClose()
    signal openDisplaySettings()
    // the closing slide is over
    signal closed()

    width: 340 + 2 * margin
    height: card.height + 2 * margin
    focus: true

    function label(mode) {
        switch (mode) {
        case "pc-only": return i18nd("raye_shell_project", "PC screen only")
        case "duplicate": return i18nd("raye_shell_project", "Duplicate")
        case "extend": return i18nd("raye_shell_project", "Extend")
        case "second-only": return i18nd("raye_shell_project", "Second screen only")
        }
        return ""
    }
    function pick(row) {
        if (controller && canChoose && !controller.busy)
            controller.choose(modes[row])
    }
    function next(step) {
        focusRow = (focusRow + step + modes.length) % modes.length
    }
    function startOnCurrent() {
        const at = controller ? modes.indexOf(controller.currentMode) : -1
        focusRow = at >= 0 ? at : 0
    }

    Component.onCompleted: startOnCurrent()
    Connections {
        target: panel.controller
        function onChanged() { panel.startOnCurrent() }
        function onActivated() { panel.next(1) }
        function onDismissed() { panel.requestClose() }
        function onChosen() { panel.requestClose() }
    }

    Keys.onUpPressed: next(-1)
    Keys.onDownPressed: next(1)
    Keys.onTabPressed: next(1)
    Keys.onBacktabPressed: next(-1)
    Keys.onReturnPressed: pick(focusRow)
    Keys.onEnterPressed: pick(focusRow)
    Keys.onSpacePressed: pick(focusRow)
    Keys.onEscapePressed: requestClose()

    // the soft shadow of the card
    Repeater {
        model: 4
        Rectangle {
            required property int index
            x: card.x - (index + 1) * 3
            y: card.y + slide.y - (index + 1) * 3 + 6
            width: card.width + 2 * (index + 1) * 3
            height: card.height + 2 * (index + 1) * 3 - 6
            radius: ShellTokens.radiusFlyout + (index + 1) * 3
            color: Qt.rgba(0, 0, 0, (Tokens.night ? 0.07 : 0.03) * slide.opacity)
        }
    }

    Item {
        id: slide
        y: panel.shown ? 0 : ShellTokens.flyoutSlide
        width: panel.width
        height: panel.height
        opacity: panel.shown ? 1 : 0
        Behavior on y {
            NumberAnimation {
                duration: panel.shown ? ShellTokens.durationSlow : ShellTokens.durationFast
                easing: panel.shown ? Motion.entrance : Motion.exit
            }
        }
        Behavior on opacity {
            SequentialAnimation {
                NumberAnimation {
                    duration: panel.shown ? ShellTokens.durationSlow : ShellTokens.durationFast
                    easing: panel.shown ? Motion.entrance : Motion.exit
                }
                ScriptAction { script: if (!panel.shown) panel.closed() }
            }
        }

        Item {
            id: card
            x: panel.margin
            y: panel.margin
            width: 340
            height: column.height

            AcrylicSurface {
                anchors.fill: parent
                focused: panel.activeFocus
            }

            Column {
                id: column
                width: parent.width
                topPadding: 12
                bottomPadding: 8
                spacing: 2

                Text {
                    x: 16
                    height: 32
                    verticalAlignment: Text.AlignVCenter
                    text: i18nd("raye_shell_project", "Project")
                    font: ShellTokens.bodyStrong
                    color: ShellTokens.text
                }

                Repeater {
                    model: panel.modes
                    Rectangle {
                        id: row
                        required property string modelData
                        required property int index
                        objectName: "row-" + modelData
                        readonly property bool inUse: !!panel.controller && panel.controller.currentMode === modelData
                        readonly property bool keysHere: panel.focusRow === index && panel.canChoose
                        readonly property string title: panel.label(modelData)
                        x: 8
                        width: parent.width - 16
                        height: 52
                        radius: ShellTokens.radiusButton
                        enabled: panel.canChoose
                        opacity: enabled || inUse ? 1 : 0.45
                        color: inUse ? Tokens.fillSubtleSelected
                             : rowArea.pressed ? ShellTokens.pressFill
                             : rowArea.containsMouse ? ShellTokens.hoverFill : "transparent"
                        Accessible.role: Accessible.RadioButton
                        Accessible.name: title
                        Accessible.checkable: true
                        Accessible.checked: inUse

                        // the bar of the row in use
                        Rectangle {
                            visible: row.inUse
                            anchors.verticalCenter: parent.verticalCenter
                            x: 0
                            width: 3
                            height: 20
                            radius: 1.5
                            color: ShellTokens.accent
                        }
                        ProjectGlyph {
                            x: 14
                            anchors.verticalCenter: parent.verticalCenter
                            mode: row.modelData
                        }
                        Text {
                            x: 72
                            width: parent.width - 72 - 32
                            anchors.verticalCenter: parent.verticalCenter
                            text: row.title
                            font: ShellTokens.body
                            color: ShellTokens.text
                            elide: Text.ElideRight
                        }
                        Kirigami.Icon {
                            visible: row.inUse
                            anchors.right: parent.right
                            anchors.rightMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            width: 16
                            height: 16
                            source: "checkmark-symbolic"
                            color: ShellTokens.accent
                            isMask: true
                        }
                        FocusRing {
                            radius: ShellTokens.radiusButton
                            target: row
                            shown: row.keysHere && panel.activeFocus
                        }
                        MouseArea {
                            id: rowArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: panel.pick(row.index)
                        }
                    }
                }

                // with one screen the rows cannot do anything: say why
                Text {
                    objectName: "oneScreenHint"
                    x: 16
                    width: parent.width - 32
                    visible: !!panel.controller && panel.controller.ready && !panel.canChoose
                    topPadding: 6
                    wrapMode: Text.WordWrap
                    text: i18nd("raye_shell_project", "Connect another screen to use these.")
                    font: ShellTokens.caption
                    color: ShellTokens.textSecondary
                }

                Item {
                    width: parent.width
                    height: 8
                }
                Rectangle {
                    x: 8
                    width: parent.width - 16
                    height: 1
                    color: ShellTokens.separator
                }
                Rectangle {
                    id: link
                    objectName: "displaySettings"
                    x: 8
                    width: parent.width - 16
                    height: 40
                    radius: ShellTokens.radiusButton
                    color: linkArea.pressed ? ShellTokens.pressFill
                         : linkArea.containsMouse ? ShellTokens.hoverFill : "transparent"
                    signal clicked()
                    onClicked: {
                        panel.openDisplaySettings()
                        panel.requestClose()
                    }
                    Accessible.role: Accessible.Link
                    Accessible.name: linkText.text
                    Row {
                        anchors.verticalCenter: parent.verticalCenter
                        x: 14
                        spacing: 10
                        Kirigami.Icon {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 16
                            height: 16
                            source: "configure-symbolic"
                            color: ShellTokens.text
                            isMask: true
                        }
                        Text {
                            id: linkText
                            anchors.verticalCenter: parent.verticalCenter
                            text: i18nd("raye_shell_project", "Display settings")
                            font: ShellTokens.body
                            color: ShellTokens.text
                        }
                    }
                    MouseArea {
                        id: linkArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: link.clicked()
                    }
                }
            }
        }
    }
}
