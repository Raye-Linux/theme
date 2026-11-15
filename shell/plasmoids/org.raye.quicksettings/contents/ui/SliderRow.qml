// One slider of the panel: a glyph that can be clicked, the slider, and
// sometimes a "›" on the right.
import QtQuick
import org.kde.kirigami as Kirigami
import org.raye.shell

Item {
    id: row

    property string iconName: ""
    property string iconTip: ""
    property string label: ""
    property int value: 0
    property bool iconClickable: false
    property bool showArrow: false

    readonly property alias pressed: slider.pressed

    signal moved(var value)
    signal iconClicked()
    signal arrowClicked()

    implicitHeight: 40

    PanelButton {
        id: glyph
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        width: 32
        height: 32
        onTaskbar: false
        enabled: row.iconClickable
        toolTipText: row.iconTip
        onClicked: row.iconClicked()
        Kirigami.Icon {
            anchors.centerIn: parent
            width: 16
            height: 16
            source: row.iconName
            color: ShellTokens.text
            isMask: true
        }
    }
    RayeSlider {
        id: slider
        anchors.left: glyph.right
        anchors.leftMargin: 4
        // every slider ends at the same place, with its arrow or without
        anchors.right: arrow.left
        anchors.rightMargin: 4
        anchors.verticalCenter: parent.verticalCenter
        from: 0
        to: 100
        stepSize: 1
        Accessible.name: row.label
        onMoved: row.moved(Math.round(value))
        // the system's value, except under the finger
        Binding on value {
            when: !slider.pressed
            value: row.value
        }
    }
    PanelButton {
        id: arrow
        opacity: row.showArrow ? 1 : 0
        enabled: row.showArrow
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: 32
        height: 32
        onTaskbar: false
        toolTipText: row.label
        onClicked: row.arrowClicked()
        Kirigami.Icon {
            anchors.centerIn: parent
            width: 12
            height: 12
            source: "arrow-right"
            color: ShellTokens.text
            isMask: true
        }
    }
}
