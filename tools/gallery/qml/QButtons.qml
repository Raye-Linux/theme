import QtQuick
import QtQuick.Controls

PageBase {
    heading: "Buttons"

    SampleRow {
        caption: "Button"
        Button { text: "Rest" }
        Button {
            objectName: "hoverTarget"
            text: "Hover"
            ToolTip.visible: hovered
            ToolTip.delay: 500
            ToolTip.text: "A tooltip shows after a short hover"
        }
        Button { text: "Pressed"; down: true }
        Button { objectName: "focusTarget"; text: "Focused" }
        Button { text: "Disabled"; enabled: false }
    }

    SampleRow {
        caption: "Accent button"
        Button { text: "Restart now"; highlighted: true }
        Button { text: "Pressed"; highlighted: true; down: true }
        Button { text: "Disabled"; highlighted: true; enabled: false }
    }

    SampleRow {
        caption: "Icon, toggle, flat"
        Button { text: "Save"; icon.name: "document-save" }
        Button { text: "Off"; checkable: true }
        Button { text: "On"; checkable: true; checked: true }
        Button { text: "Flat"; flat: true }
        Button { text: "Flat disabled"; flat: true; enabled: false }
    }

    SampleRow {
        caption: "Tool buttons"
        ToolButton { icon.name: "edit-cut"; text: "Cut"; display: AbstractButton.IconOnly }
        ToolButton { icon.name: "format-text-bold"; text: "Bold"; display: AbstractButton.IconOnly; checkable: true; checked: true }
        ToolButton { icon.name: "edit-copy"; text: "Copy"; display: AbstractButton.IconOnly; down: true }
        ToolButton { icon.name: "edit-paste"; text: "Paste"; display: AbstractButton.IconOnly; enabled: false }
        ToolButton { icon.name: "view-sort"; text: "Sort" }
    }

    SampleRow {
        caption: "Round, delay"
        RoundButton { icon.name: "list-add"; text: "Add"; display: AbstractButton.IconOnly }
        DelayButton { text: "Hold to confirm"; progress: 0.4 }
    }

    SampleRow {
        caption: "Dialog buttons"
        DialogButtonBox {
            standardButtons: DialogButtonBox.Ok | DialogButtonBox.Cancel | DialogButtonBox.Apply
        }
    }
}
