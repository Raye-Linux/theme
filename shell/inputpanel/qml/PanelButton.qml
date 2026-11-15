// A flat button of the panel: a glyph, a hover fill, and when selected a
// fill with a short accent pill under it.
import QtQuick
import QtQuick.Shapes
import QtQuick.Templates as T

T.AbstractButton {
    id: button

    property bool selected: false
    property bool glyphClose: false
    property string toolTip: ""
    property bool tipAbove: false
    property color glyphColor: Tokens.textPrimary

    focusPolicy: Qt.TabFocus
    hoverEnabled: true
    Accessible.name: toolTip.length > 0 ? toolTip : text
    Accessible.role: Accessible.Button

    background: Rectangle {
        radius: Tokens.radiusControl
        color: button.down ? Tokens.fillSubtlePressed
             : button.selected ? Tokens.fillSubtleSelected
             : button.hovered ? Tokens.fillSubtleHover : "transparent"
        Behavior on color { ColorAnimation { duration: Tokens.durationFaster } }

        Rectangle {
            anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 2 }
            width: button.selected ? 16 : 0
            height: 3
            radius: 1.5
            color: Tokens.accent
            Behavior on width { NumberAnimation { duration: Tokens.durationFast; easing.type: Tokens.easingFast } }
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: -2
            radius: Tokens.radiusControl + 2
            color: "transparent"
            border.width: Tokens.sizeFocusOuter
            border.color: Tokens.focusOuter
            visible: button.visualFocus
        }
    }

    contentItem: Item {
        Text {
            anchors.centerIn: parent
            visible: !button.glyphClose
            text: button.text
            font: button.font
            color: button.glyphColor
        }
        Shape {
            anchors.centerIn: parent
            visible: button.glyphClose
            width: 10
            height: 10
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeWidth: 1
                strokeColor: button.glyphColor
                fillColor: "transparent"
                startX: 0; startY: 0
                PathLine { x: 10; y: 10 }
                PathMove { x: 10; y: 0 }
                PathLine { x: 0; y: 10 }
            }
        }
    }

    // A small tooltip of our own: the panel draws all of its surface.
    Rectangle {
        id: tip
        visible: button.hovered && button.toolTip.length > 0 && tipTimer.done
        z: 10
        x: Math.round((button.width - width) / 2)
        y: button.tipAbove ? -height - 4 : button.height + 4
        width: tipText.implicitWidth + Tokens.sizeTooltipPaddingLeft + Tokens.sizeTooltipPaddingRight
        height: tipText.implicitHeight + Tokens.sizeTooltipPaddingTop + Tokens.sizeTooltipPaddingBottom
        radius: Tokens.radiusTooltip
        color: Tokens.bgTooltip
        border.width: 1
        border.color: Tokens.strokeSurface
        Text {
            id: tipText
            anchors.centerIn: parent
            text: button.toolTip
            font: Tokens.caption
            color: Tokens.textPrimary
        }
    }
    Timer {
        id: tipTimer
        property bool done: false
        interval: 600
        running: button.hovered
        onRunningChanged: if (!running) done = false
        onTriggered: done = true
    }
}
