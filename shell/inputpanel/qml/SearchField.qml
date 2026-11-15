// The search box: 36 tall, a magnifier, the accent line while typing.
import QtQuick
import QtQuick.Controls.Basic as Basic
import QtQuick.Shapes

Basic.TextField {
    id: field

    implicitHeight: 36
    font: Tokens.body
    color: Tokens.textPrimary
    placeholderTextColor: Tokens.textSecondary
    selectionColor: Tokens.accent
    selectedTextColor: Tokens.textOnAccent
    leftPadding: 36
    rightPadding: Tokens.s4
    verticalAlignment: TextInput.AlignVCenter
    selectByMouse: true
    Accessible.name: placeholderText

    background: Rectangle {
        radius: Tokens.radiusControl
        color: field.activeFocus ? Tokens.fillInputActive
             : field.hovered ? Tokens.fillControlHover : Tokens.fillControl
        border.width: 1
        border.color: Tokens.strokeControl

        Rectangle {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            anchors.leftMargin: field.activeFocus ? 0 : 1
            anchors.rightMargin: anchors.leftMargin
            height: field.activeFocus ? 2 : 1
            bottomLeftRadius: Tokens.radiusControl
            bottomRightRadius: Tokens.radiusControl
            color: field.activeFocus ? Tokens.accent : Tokens.strokeControlBottom
        }

        // magnifier
        Shape {
            x: 12
            anchors.verticalCenter: parent.verticalCenter
            width: 14
            height: 14
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeWidth: 1.25
                strokeColor: Tokens.textSecondary
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                PathAngleArc { centerX: 6; centerY: 6; radiusX: 5; radiusY: 5; startAngle: 0; sweepAngle: 360 }
            }
            ShapePath {
                strokeWidth: 1.25
                strokeColor: Tokens.textSecondary
                capStyle: ShapePath.RoundCap
                startX: 9.8; startY: 9.8
                PathLine { x: 13.5; y: 13.5 }
            }
        }
    }
}
