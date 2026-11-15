import QtQuick
import QtQuick.Controls

PageBase {
    heading: "Check boxes, radios, switches"

    SampleRow {
        caption: "Check box"
        CheckBox { text: "Off" }
        CheckBox { text: "On"; checked: true }
        CheckBox { text: "Mixed"; tristate: true; checkState: Qt.PartiallyChecked }
        CheckBox { objectName: "hoverTarget"; text: "Hover" }
        CheckBox { objectName: "focusTarget"; text: "Focused"; checked: true }
    }

    SampleRow {
        caption: "Check box, disabled"
        CheckBox { text: "Off"; enabled: false }
        CheckBox { text: "On"; checked: true; enabled: false }
        CheckBox { text: "Mixed"; tristate: true; checkState: Qt.PartiallyChecked; enabled: false }
    }

    SampleRow {
        caption: "Radio button"
        ButtonGroup { id: group }
        RadioButton { text: "Off"; ButtonGroup.group: group }
        RadioButton { text: "On"; checked: true; ButtonGroup.group: group }
        RadioButton { text: "Off, disabled"; autoExclusive: false; enabled: false }
        RadioButton { text: "On, disabled"; autoExclusive: false; checked: true; enabled: false }
    }

    SampleRow {
        caption: "Switch"
        Switch { text: "Off" }
        Switch { text: "On"; checked: true }
        Switch { text: "Off, disabled"; enabled: false }
        Switch { text: "On, disabled"; checked: true; enabled: false }
    }

    SampleRow {
        caption: "Combo box"
        ComboBox { model: ["Recommended", "Balanced", "Best performance"] }
        ComboBox { model: ["Recommended", "Balanced", "Best performance"]; editable: true }
        ComboBox { model: ["Recommended"]; enabled: false }
    }
}
