import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

PageBase {
    heading: "Text and number fields"

    SampleRow {
        caption: "Text field"
        TextField { placeholderText: "Search"; implicitWidth: 180 }
        TextField { objectName: "hoverTarget"; text: "Hover"; implicitWidth: 180 }
        TextField { objectName: "focusTarget"; text: "Focused"; implicitWidth: 180 }
    }

    SampleRow {
        caption: "More states"
        TextField { text: "Read only"; readOnly: true; implicitWidth: 180 }
        TextField { text: "Disabled"; enabled: false; implicitWidth: 180 }
        TextField { text: "password"; echoMode: TextInput.Password; implicitWidth: 180 }
    }

    SampleRow {
        caption: "Number field"
        SpinBox { from: 0; to: 200; value: 42 }
        SpinBox { from: 0; to: 200; value: 120; editable: true }
        SpinBox { value: 7; enabled: false }
    }

    SampleRow {
        caption: "Text area"
        ScrollView {
            Layout.preferredWidth: 420
            Layout.preferredHeight: 110
            TextArea {
                text: "A text area with a few lines.\nSelect, type, scroll.\nThe frame and the focus line come from the style.\nLine four\nLine five\nLine six\nLine seven"
            }
        }
    }
}
