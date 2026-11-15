// The view of All: Category, Grid or List, three small buttons next to the
// heading. The choice is kept in rayeshellrc, [Start] AllView.
import QtQuick
import QtQuick.Templates as T
import org.raye.ui
import org.raye.shell

Row {
    id: picker

    property string current: "category"
    readonly property Item currentButton: buttons.itemAt(Math.max(0, kinds.indexOf(current)))
    readonly property var kinds: ["category", "grid", "list"]
    signal picked(string kind)

    spacing: 2

    function label(kind) {
        switch (kind) {
        case "category": return i18nd("raye_shell_start", "Category")
        case "grid": return i18nd("raye_shell_start", "Grid")
        case "list": return i18nd("raye_shell_start", "List")
        }
        return ""
    }

    Repeater {
        id: buttons
        model: picker.kinds
        delegate: T.AbstractButton {
            id: button
            required property string modelData
            required property int index
            readonly property bool selected: picker.current === modelData
            objectName: "viewPicker_" + modelData
            implicitWidth: name.implicitWidth + 20
            implicitHeight: 28
            hoverEnabled: true
            focusPolicy: Qt.TabFocus
            checkable: false
            Accessible.role: Accessible.RadioButton
            Accessible.name: name.text
            Accessible.checked: selected
            onClicked: picker.picked(modelData)
            Keys.onReturnPressed: clicked()
            Keys.onLeftPressed: event => {
                if (index > 0) {
                    picker.picked(picker.kinds[index - 1])
                    buttons.itemAt(index - 1).forceActiveFocus(Qt.TabFocusReason)
                }
            }
            Keys.onRightPressed: event => {
                if (index < picker.kinds.length - 1) {
                    picker.picked(picker.kinds[index + 1])
                    buttons.itemAt(index + 1).forceActiveFocus(Qt.TabFocusReason)
                }
            }
            background: Rectangle {
                radius: Tokens.radiusControl
                color: button.selected ? Qt.alpha(ShellTokens.accent, 0.3)
                     : button.down ? ShellTokens.pressFill
                     : button.hovered ? ShellTokens.hoverFill : "transparent"
                FocusRing { radius: Tokens.radiusControl; target: button }
            }
            contentItem: Text {
                id: name
                text: picker.label(button.modelData)
                font: Tokens.caption
                color: ShellTokens.text
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
    }
}
