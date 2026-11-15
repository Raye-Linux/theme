// The toggle switch the Windows way: a 40 x 20 track, a 12 px knob that
// grows to 14 under the pointer and stretches to 17 x 14 while pressed,
// and "On" / "Off" 12 px to the right of the track.
//
//   Switch { text: qsTr("Bluetooth"); checked: bt.enabled; onToggled: ... }
//
// text is the name read to screen readers; set showStateText: false where
// the row already says what the switch does and only the track is wanted.
import QtQuick
import QtQuick.Templates as T
import org.raye.ui

T.Switch {
    id: control

    property bool showStateText: true
    // "On" and "Off" in the language of the system
    readonly property bool french: Qt.locale().name.startsWith("fr")
    property string onText: french ? "Activé" : "On"
    property string offText: french ? "Désactivé" : "Off"

    readonly property bool knobHover: hovered && !down
    readonly property real knobWidth: down ? Tokens.sizeToggleKnobPressedWidth
                                    : knobHover ? Tokens.sizeToggleKnobHover : Tokens.sizeToggleKnob
    readonly property real knobHeight: down ? Tokens.sizeToggleKnobPressedHeight
                                     : knobHover ? Tokens.sizeToggleKnobHover : Tokens.sizeToggleKnob

    // the text item starts after the track (its left padding), so its
    // width already holds the track and the gap
    implicitWidth: (showStateText ? implicitContentWidth : implicitIndicatorWidth)
                   + leftPadding + rightPadding
    implicitHeight: Math.max(Tokens.sizeControlHeight, implicitIndicatorHeight + topPadding + bottomPadding)
    padding: 0
    spacing: Tokens.s4
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    font: Tokens.body

    Accessible.role: Accessible.CheckBox
    Accessible.name: text !== "" ? text : (checked ? onText : offText)
    Accessible.checkable: true
    Accessible.checked: checked

    indicator: Rectangle {
        id: track
        implicitWidth: Tokens.sizeToggleWidth
        implicitHeight: Tokens.sizeToggleHeight
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        radius: height / 2
        color: !control.enabled ? (control.checked ? Tokens.accentDisabled : "transparent")
             : control.checked ? (control.down ? Tokens.accentPressed
                                : control.hovered ? Tokens.accentHover : Tokens.accent)
             : control.down ? Tokens.fillSubtlePressed
             : control.hovered ? Tokens.fillSubtleHover : Tokens.fillControlPressed
        border.width: control.checked ? 0 : 1
        border.color: control.enabled ? Tokens.strokeStrong : Tokens.textDisabled

        Behavior on color {
            ColorAnimation { duration: Tokens.durationFaster }
        }

        Rectangle {
            id: knob
            width: control.knobWidth
            height: control.knobHeight
            radius: height / 2
            y: (track.height - height) / 2
            // the knob keeps the same gap to the end of the track it rests at
            readonly property real gap: (track.height - height) / 2
            x: control.checked ? track.width - width - gap : gap
            color: !control.enabled ? (control.checked ? Qt.alpha(Tokens.textOnAccent, 0.6) : Tokens.textDisabled)
                 : control.checked ? Tokens.textOnAccent : Tokens.textSecondary

            Behavior on x {
                NumberAnimation { duration: Tokens.durationFast; easing.type: Tokens.easingFast }
            }
            Behavior on width {
                NumberAnimation { duration: Tokens.durationFaster }
            }
            Behavior on height {
                NumberAnimation { duration: Tokens.durationFaster }
            }
        }

        FocusRing {
            target: control
            radius: track.radius
        }
    }

    contentItem: Text {
        leftPadding: control.indicator.width + control.spacing
        visible: control.showStateText
        text: control.checked ? control.onText : control.offText
        font: control.font
        color: control.enabled ? Tokens.textPrimary : Tokens.textDisabled
        verticalAlignment: Text.AlignVCenter
    }
}
