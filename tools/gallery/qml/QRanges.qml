import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

PageBase {
    heading: "Sliders and progress"

    SampleRow {
        caption: "Slider"
        Slider { Layout.preferredWidth: 180; from: 0; to: 100; value: 40 }
        Slider { Layout.preferredWidth: 180; objectName: "hoverTarget"; from: 0; to: 100; value: 60 }
        Slider { Layout.preferredWidth: 180; objectName: "focusTarget"; from: 0; to: 100; value: 30 }
        Slider { Layout.preferredWidth: 180; from: 0; to: 100; value: 50; enabled: false }
    }

    SampleRow {
        caption: "Steps, range"
        Slider { Layout.preferredWidth: 180; from: 0; to: 10; stepSize: 1; snapMode: Slider.SnapAlways; value: 4 }
        RangeSlider { Layout.preferredWidth: 220; from: 0; to: 100; first.value: 20; second.value: 70 }
    }

    SampleRow {
        caption: "Vertical"
        Slider { orientation: Qt.Vertical; value: 0.25; Layout.preferredHeight: 120 }
        Slider { orientation: Qt.Vertical; value: 0.75; Layout.preferredHeight: 120 }
        Slider { orientation: Qt.Vertical; value: 0.5; enabled: false; Layout.preferredHeight: 120 }
    }

    SampleRow {
        caption: "Progress bar"
        ProgressBar { Layout.preferredWidth: 160; value: 0 }
        ProgressBar { Layout.preferredWidth: 160; value: 0.3 }
        ProgressBar { Layout.preferredWidth: 160; value: 0.7 }
        ProgressBar { Layout.preferredWidth: 160; value: 1 }
    }

    SampleRow {
        caption: "Busy, disabled"
        ProgressBar { Layout.preferredWidth: 160; indeterminate: true }
        ProgressBar { Layout.preferredWidth: 160; value: 0.6; enabled: false }
        BusyIndicator { running: true }
        BusyIndicator { running: false }
    }

    SampleRow {
        caption: "Dial"
        Dial { value: 0.35 }
        Dial { value: 0.7; enabled: false }
    }
}
