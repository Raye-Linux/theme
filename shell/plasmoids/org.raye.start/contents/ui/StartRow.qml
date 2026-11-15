// The Start applet on the taskbar: the Raye eye, the search field or
// button (per the search style of Settings) and the Task view button.
// Each is a 40 x 40 backplate with 4 px corners; the search box is a
// 232 x 32 pill. `leadingSpace` is the room left before the eye so the
// whole group (these buttons and the apps after them) sits in the middle
// of the screen.
import QtQuick
import QtQuick.Templates as T
import org.kde.kirigami as Kirigami
import org.raye.ui
import org.raye.shell

import "startlogic.js" as Logic

Item {
    id: row

    property var settings: ShellSettings
    property int leadingSpace: 0
    // the popup is open, in which mode
    property bool expanded: false
    property string mode: "start"

    readonly property string searchStyle: Logic.searchStyle(settings.searchMode)
    readonly property bool taskViewShown: settings.taskView
    readonly property int rowWidth: Logic.rowWidth(searchStyle, taskViewShown)

    readonly property Item startButton: startButton
    readonly property Item taskViewButton: taskViewButton

    signal startClicked()
    signal searchClicked()
    signal taskViewClicked()
    signal taskViewHovered(bool hovered)
    // right-click on the eye: the quick link menu, at that point of the eye
    signal quickLinksRequested()
    // the quick link menu is open: no "Start" tooltip over it
    property bool quickLinksOpen: false

    implicitWidth: leadingSpace + rowWidth
    implicitHeight: ShellTokens.taskbarHeight
    Behavior on leadingSpace {
        NumberAnimation { duration: ShellTokens.durationNormal; easing: Motion.pointToPoint }
    }

    Row {
        id: buttons
        x: row.leadingSpace
        anchors.verticalCenter: parent.verticalCenter
        spacing: Logic.buttonGap

        // the Raye eye, which blinks when pressed
        PanelButton {
            id: startButton
            objectName: "startButton"
            selected: row.expanded && row.mode === "start"
            toolTipText: row.expanded || row.quickLinksOpen ? "" : i18nd("raye_shell_start", "Start")
            Accessible.role: Accessible.ButtonMenu

            function blink() {
                blinkAnimation.restart()
            }

            onPressed: blink()
            onClicked: row.startClicked()

            Kirigami.Icon {
                id: eye
                objectName: "eye"
                // 1 open, near 0 closed: the lid squeezes the eye from top and bottom
                property real lid: 1
                anchors.centerIn: parent
                // the eye is wide and flat: 32 wide gives it the weight of
                // a 24 px square glyph
                width: 32
                height: 32
                source: "raye-eye"
                transform: Scale {
                    origin.x: eye.width / 2
                    origin.y: eye.height / 2
                    yScale: eye.lid
                }

                SequentialAnimation {
                    id: blinkAnimation
                    NumberAnimation { target: eye; property: "lid"; to: 0.1; duration: ShellTokens.blinkClosed; easing.type: Easing.InQuad }
                    NumberAnimation { target: eye; property: "lid"; to: 1; duration: ShellTokens.blinkOpen; easing: Motion.entrance }
                }
            }

            TapHandler {
                acceptedButtons: Qt.RightButton
                onTapped: row.quickLinksRequested()
            }
        }

        T.AbstractButton {
            id: searchBox
            objectName: "searchBox"
            visible: row.searchStyle === "box"
            width: Logic.searchBoxWidth
            height: Logic.searchBoxHeight
            anchors.verticalCenter: parent.verticalCenter
            hoverEnabled: true
            focusPolicy: Qt.TabFocus
            Accessible.role: Accessible.EditableText
            Accessible.name: i18nd("raye_shell_start", "Search")
            onClicked: row.searchClicked()

            background: Rectangle {
                radius: height / 2
                color: ShellTokens.taskbarDark ? Qt.rgba(1, 1, 1, searchBox.down ? 0.04 : searchBox.hovered ? 0.08 : 0.05)
                                               : Qt.rgba(1, 1, 1, searchBox.hovered ? 0.7 : 0.55)
                border.width: 1
                border.color: row.expanded && row.mode === "search" ? Qt.alpha(ShellTokens.accent, 0.8)
                                                                     : ShellTokens.taskbarDark ? Qt.rgba(1, 1, 1, 0.1) : Qt.rgba(0, 0, 0, 0.08)
                Behavior on color { ColorAnimation { duration: ShellTokens.durationFaster } }
                FocusRing { radius: height / 2; target: searchBox }
            }

            contentItem: Item {
                SearchGlyph {
                    id: boxGlass
                    x: 12
                    anchors.verticalCenter: parent.verticalCenter
                    width: 16
                    height: 16
                    color: ShellTokens.taskbarText
                    tilted: searchBox.hovered
                }
                Text {
                    anchors { left: boxGlass.right; leftMargin: 10; right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
                    text: i18nd("raye_shell_start", "Search")
                    elide: Text.ElideRight
                    color: Qt.alpha(ShellTokens.taskbarText, 0.72)
                    font.family: "Inter"
                    font.pixelSize: 14
                }
            }
        }

        PanelButton {
            id: searchLabel
            objectName: "searchLabel"
            visible: row.searchStyle === "iconlabel"
            width: Logic.searchLabelWidth
            selected: row.expanded && row.mode === "search"
            Accessible.name: i18nd("raye_shell_start", "Search")
            onClicked: row.searchClicked()

            Row {
                anchors.centerIn: parent
                spacing: 8
                SearchGlyph {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 16
                    height: 16
                    color: ShellTokens.taskbarText
                    tilted: searchLabel.hovered
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: i18nd("raye_shell_start", "Search")
                    color: ShellTokens.taskbarText
                    font: ShellTokens.caption
                    elide: Text.ElideRight
                    width: Math.min(implicitWidth, searchLabel.width - 44)
                }
            }
        }

        PanelButton {
            id: searchIcon
            objectName: "searchIcon"
            visible: row.searchStyle === "icon"
            selected: row.expanded && row.mode === "search"
            toolTipText: i18nd("raye_shell_start", "Search")
            onClicked: row.searchClicked()

            SearchGlyph {
                anchors.centerIn: parent
                width: 20
                height: 20
                color: ShellTokens.taskbarText
                tilted: searchIcon.hovered
            }
        }

        PanelButton {
            id: taskViewButton
            objectName: "taskViewButton"
            visible: row.taskViewShown
            toolTipText: i18nd("raye_shell_start", "Task view")
            onClicked: row.taskViewClicked()
            onHoveredChanged: row.taskViewHovered(hovered)

            TaskViewGlyph {
                anchors.centerIn: parent
                width: ShellTokens.iconSize
                height: ShellTokens.iconSize
                color: ShellTokens.taskbarText
                slide: taskViewButton.hovered ? 1.5 : 0
                Behavior on slide { NumberAnimation { duration: ShellTokens.durationFast; easing: Motion.pointToPoint } }
            }
        }
    }
}
