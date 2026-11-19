// A button of the taskbar: a 40 x 40 backplate with 4 px corners that
// shows on hover, lighter while pressed, firmer when the button is on; the
// content shrinks to 90 % while pressed. Put an icon in it, or anything:
//
//   PanelButton { icon.name: "raye-eye"; checked: start.expanded; onClicked: ... }
//   PanelButton { width: 160; Row { ... } }
import QtQuick
import QtQuick.Templates as T
import org.kde.kirigami as Kirigami
import org.raye.ui
import org.raye.shell

T.AbstractButton {
    id: control

    // the content to scale while pressed: the icon, or what the user put in
    default property alias content: holder.data
    property int iconSize: ShellTokens.iconSize
    // "on" without a click (an open flyout, the app in use)
    property bool selected: checked
    property string toolTipText: ""
    // on the taskbar (its own shade) or on a flyout (the page colours)
    property bool onTaskbar: true
    // room around the button that still takes its clicks (see EdgeReach)
    property real reachLeft: 0
    property real reachTop: 0
    property real reachRight: 0
    property real reachBottom: 0

    implicitWidth: ShellTokens.buttonSize
    implicitHeight: ShellTokens.buttonSize
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    containmentMask: EdgeReach {
        target: control
        left: control.reachLeft
        top: control.reachTop
        right: control.reachRight
        bottom: control.reachBottom
    }
    Accessible.name: toolTipText !== "" ? toolTipText : text

    background: Rectangle {
        radius: ShellTokens.radiusButton
        color: control.down ? (control.onTaskbar ? ShellTokens.taskbarPressFill : ShellTokens.pressFill)
             : control.selected ? (control.onTaskbar ? ShellTokens.taskbarSelectedFill : ShellTokens.selectedFill)
             : control.hovered ? (control.onTaskbar ? ShellTokens.taskbarHoverFill : ShellTokens.hoverFill) : "transparent"
        Behavior on color { ColorAnimation { duration: ShellTokens.durationFaster } }

        FocusRing {
            radius: ShellTokens.radiusButton
            target: control
        }
    }

    contentItem: Item {
        Item {
            id: holder
            anchors.fill: parent
            scale: control.down ? ShellTokens.pressScale : 1
            Behavior on scale { NumberAnimation { duration: ShellTokens.durationFaster; easing: Motion.pointToPoint } }

            Kirigami.Icon {
                anchors.centerIn: parent
                width: control.iconSize
                height: control.iconSize
                visible: source !== ""
                source: control.icon.name !== "" ? control.icon.name : control.icon.source
                active: control.hovered
            }
        }
    }

    RayeTooltip {
        visible: control.hovered && !control.down && control.toolTipText !== ""
        text: control.toolTipText
    }
}
