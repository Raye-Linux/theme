// The slider of quick settings (volume, brightness): a 4 px track, the
// accent up to the value, a 20 px thumb with an accent dot that grows on
// hover and shrinks while dragged.
//
//   RayeSlider { from: 0; to: 100; value: volume; onMoved: setVolume(value) }
import QtQuick
import QtQuick.Templates as T
import org.raye.ui
import org.raye.shell

T.Slider {
    id: control

    implicitWidth: 200
    implicitHeight: Tokens.sizeSliderThumb
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.role: Accessible.Slider

    background: Item {
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        width: control.availableWidth
        height: Tokens.sizeSliderTrack

        Rectangle {
            anchors.fill: parent
            radius: height / 2
            color: Tokens.night ? Qt.rgba(1, 1, 1, 0.5) : Qt.rgba(0, 0, 0, 0.45)
        }
        Rectangle {
            objectName: "sliderFill"
            width: control.visualPosition * parent.width
            height: parent.height
            radius: height / 2
            color: control.enabled ? ShellTokens.accent : Tokens.accentDisabled
        }
    }

    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + (control.availableHeight - height) / 2
        width: Tokens.sizeSliderThumb
        height: Tokens.sizeSliderThumb
        radius: width / 2
        color: Tokens.night ? "#454545" : "#ffffff"
        border.width: 1
        border.color: Tokens.strokeControl

        Rectangle {
            anchors.centerIn: parent
            readonly property int size: control.pressed ? Tokens.sizeSliderThumbInnerPressed
                                      : control.hovered ? Tokens.sizeSliderThumbInnerHover
                                      : Tokens.sizeSliderThumbInner
            width: size
            height: size
            radius: size / 2
            color: control.enabled ? ShellTokens.accent : Tokens.accentDisabled
            Behavior on width { NumberAnimation { duration: ShellTokens.durationFaster } }
            Behavior on height { NumberAnimation { duration: ShellTokens.durationFaster } }
        }

        FocusRing {
            radius: width / 2
            target: control
        }
    }
}
