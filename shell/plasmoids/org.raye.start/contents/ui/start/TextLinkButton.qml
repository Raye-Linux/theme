// The small button at the right of a section header: "All apps ›",
// "‹ Back", "More ›". A filled pill with 4 px corners, Caption text.
import QtQuick
import QtQuick.Templates as T
import org.raye.ui
import org.raye.shell

T.AbstractButton {
    id: button

    property bool back: false

    implicitWidth: label.implicitWidth + 24
    implicitHeight: 24
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.name: text

    background: Rectangle {
        radius: Tokens.radiusControl
        color: button.down ? Tokens.fillControlPressed : button.hovered ? Tokens.fillControlHover : Tokens.fillControl
        border.width: 1
        border.color: Tokens.strokeControl
        FocusRing { radius: Tokens.radiusControl; target: button }
    }
    contentItem: Text {
        id: label
        text: button.back ? "‹  " + button.text : button.text + "  ›"
        font: Tokens.caption
        color: ShellTokens.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
